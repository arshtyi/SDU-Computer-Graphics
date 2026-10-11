#import "@preview/numbly:0.1.0": numbly
#import "@preview/pointless-size:0.1.2": zh, zihao
#import "@preview/codly:1.3.0": *
#import "@preview/codly-languages:0.1.10": *

#let institute = "计算机科学与技术"
#let course = "计算机图形学"
#let author = "彭靖轩"
#let id = "202400130242"
#let class = "24智能"
#let email = link("mailto:arshtyi@foxmail.com")
#let date = datetime.today()
#let title = "实验4：二维的 Catmull-Clark 细分方法"

#set document(title: title, author: author, date: date)
#set text(font: ((name: "lato", covers: "latin-in-cjk"), "noto serif cjk sc"), size: zh(5), lang: "zh", region: "cn")
#set par(justify: true, first-line-indent: (amount: 2em, all: true))
#set page(
    paper: "a4",
    margin: (x: 35pt, y: 35pt),
    footer: align(center, context counter(page).display("- 1 -")),
)
#set heading(numbering: numbly("", "{2:1}.", "({3:1})"))
#show heading: set text(size: zh(-4))
#{
    set underline(offset: 2.5pt, extent: 2.5pt)
    show heading: it => align(center, text(tracking: .1em, size: zh(-2), it))
    heading(numbering: none, level: 1)[山东大学 #underline[#institute] 学院\ #underline[#course] 课程实验报告]
    set text(size: zh(-4))
    set table.cell(inset: .5em, align: left + horizon, stroke: 1pt)
    table(
        columns: (3fr, auto),
        [实验题目：#title], [学号：#id],
    )
    v(0em, weak: true)
    table(
        columns: (3fr, 2.5fr, 3fr),
        [日期：#date.display("[year].[month].[day]")], [班级：#class], [姓名：#author],
    )
    v(0em, weak: true)
    table(
        columns: 1fr,
        [Email：#email]
    )
}
#show raw: set text(font: ("JetBrains Mono", "Noto Serif CJK SC"))
#show raw.where(block: false): box.with(
    fill: luma(240),
    inset: (x: .3em, y: 0em),
    outset: (x: 0em, y: .3em),
    radius: .2em,
)
#show: codly-init
#codly(
    languages: codly-languages,
    zebra-fill: none,
    fill: luma(90.2%),
    stroke: .5pt + rgb("bfbfbf"),
    radius: 8pt,
)
#set enum(numbering: numbly("{1:1})", "{2:a}."))
#set list(indent: 10pt, marker: sym.bullet.tri)

#let in-block(body) = {
    let is-level-1-heading(it) = (
        it.func() == heading
            and (
                it.at("level", default: none) == 1
                    or (it.at("offset", default: none) + it.at("depth", default: none) == 1)
            )
    )

    let text-block(it) = {
        v(0em, weak: true)
        block(
            width: 100%,
            inset: (x: 4pt, y: 1em),
            stroke: 1pt,
            breakable: true,
            it,
        )
    }

    let children = body.at("children", default: (body,))
    let content = ()
    let buf = ()

    for child in children {
        if is-level-1-heading(child) {
            if buf.len() > 0 {
                content.push(text-block(buf.join()))
                buf = ()
            }
            buf.push(child)
        } else if buf.len() > 0 {
            buf.push(child)
        } else {
            content.push(child)
        }
    }
    if buf.len() > 0 {
        content.push(text-block(buf.join()))
    }
    content.join()
}
#show: in-block

#let data-table(path) = {
    let rows = csv(path)
    table(
        columns: (auto, 1fr, 1fr, 1fr, 1fr, 1fr),
        align: center + horizon,
        inset: 6pt,
        stroke: .5pt + luma(65%),
        fill: (x, y) => if y == 0 { luma(93%) } else { none },
        table.header([轮次], [顶点 $V$], [边 $E$], [面 $F$], [边界边 $B$], [$chi$]),
        ..rows.slice(1).flatten(),
    )
}
#let pair(name, caption) = figure(
    grid(
        columns: (1fr, 1fr),
        gutter: 8pt,
        image("image/" + name + "_level0.png"), image("image/" + name + "_level3.png"),
    ),
    caption: caption,
)

= 实验目的

- 完成指定二维网格的 Catmull-Clark 细分，理解面点、边点、旧顶点更新及拓扑重建。
- 实现 OBJ 文件读写，分别存储点、边、面，建立顶点和边的一邻域查询结构。
- 以 OpenCV 展示连续迭代过程，生成可复现的网格文件、图片和统计表，并拓展到不规则网格、多边形和带孔网格。

= 实验环境介绍

- macOS: 26.6.2
- Apple clang: 21.0.0，C++17
- OpenCV: 4.14.0
- xmake: 3.1.1

= 解决问题的主要思路

== 输入数据与邻接结构

`required.obj` 逐项保留 16 个顶点、25 条边和 10 个面。中心方格被边 $(6,11)$ 分成两个三角形，因此共有 8 个四边形、2 个三角形；点 6、11 为五价内部点，点 7、10 为四价内部点，其余 12 个点位于边界。

`Mesh` 保存双精度二维坐标和每个面的有序顶点索引。`Topology` 保存无向边、边的邻接面、面的有序边、顶点的邻接边与邻接面。用 $(min(a, b),max(a, b))$ 作为边的唯一键，共享边仅生成一个边点。内部边有两个邻接面，边界边只有一个；顶点的面扇必须连通。

文件使用从 1 开始的索引，读入后转为从 0 开始。材料中的 `e` 是自定义记录，程序兼容读取并核对完整边集合；无 `e` 时由 `f` 推导所有边。输出采用标准 OBJ 的 `v x y 0` 和 `f`。使用补充材料提供的面。

== 面点和边点

一个包含 $k$ 个顶点的面，其面点为顶点的算术平均：
$ F_f = 1/k sum_(i=0)^(k-1) P_i. $
这是控制顶点平均。三角形除以 3，四边形除以 4，任意多边形使用实际顶点数。对内部边 $e=(P_a,P_b)$，设相邻两面点为 $F_1,F_2$：
$ E_e = (P_a + P_b + F_1 + F_2)/4. $

== 内部顶点

设内部点 $P$ 的价数为 $n$，$F_i$ 为相邻面点，$R_i$ 为相邻*旧边的中点*。记 $overline(F)=sum_i F_i/n$、$overline(R)=sum_i R_i/n$，标准更新为：

$
    P' = (overline(F) + 2 overline(R) + (n-3)P)/n
    = (sum_i F_i + 2 sum_i R_i + n(n-3)P)/n^2.
$

$n=4$ 时化为 $(sum F_i+2 sum R_i+4P)/16$，$n=5$ 时为：

$ P' = (sum_(i=1)^5 F_i + 2 sum_(i=1)^5 R_i + 10P)/25. $

指定网格的点 6 为 $(1,1)$，其相邻面点之和、旧边中点之和均为 $(5.5,5.5)$，因此 $overline(F)=overline(R)=(1.1,1.1)$。正确结果为 $(1.06,1.06)$。

== 边界条件

选择平滑边界，即 OpenSubdiv 的 edge-only 边界方式：边界边点取端点中点，边界顶点按三次 B 样条规则更新。若 $Q_1,Q_2$ 是沿边界相连的两个旧邻点，则

$ E_e=(P_a+P_b)/2, quad P'=(6P+Q_1+Q_2)/8. $

只使用两个边界邻点，不把内部邻点加入该式。角点也会被平滑，不固定在原位置；例如点 1 从 $(0,0)$ 移到 $(1/8,1/8)$。直线边界上的等间距点在首轮保持位置。带孔网格的内外边界都按同一规则处理。

== 同步更新与拓扑重建

所有位置计算均读取旧网格，结果写入新数组。新点索引依次为更新后的 $V$ 个旧顶点、$E$ 个边点、$F$ 个面点。旧面 $(v_0,dots,v_(k-1))$ 的第 $i$ 个角生成四边形

$ (v_i', E_(i,i+1), F_f, E_(i-1,i)). $

索引循环取模，继承旧面绕序。每个 $k$ 边形生成 $k$ 个四边形，再从新面重建全部边和邻接。只更新线段而不生成新面，无法正确执行下一轮。

令 $C=sum_f k_f$ 为总面角数，则 $V'=V+E+F$、$F'=C$、$E'=2E+C$，边界边数 $B'=2B$，故 $V'-E'+F'=V-E+F$。首轮后全部为四边形，此后面数每轮乘 4。

几何更新用一邻域累加，耗时 $O(V+E+F+C)$。边去重使用有序映射，拓扑构建包含对数开销；单面自交检查为 $O(k_f^2)$。对本实验的小度数面和有界价数网格，每轮总时间约为 $O((V+E+F) log(V+E+F))$，空间为 $O(V+E+F)$。限制迭代次数及网格规模，避免无界增长。

= 实验步骤与实验结果

== 核心细分实现

#raw(block: true, read("../lib/catmull_clark.cpp"), lang: "cpp")
#raw(block: true, read("../xmake.lua"), lang: "lua")

== 指定网格的数量与坐标检查

第 0 轮是输入网格，第 1 至 3 轮是依次细分的结果。

#figure(data-table("data/required_stats.csv"), caption: [指定网格的实测拓扑统计])

首轮 $F'=8 times 4+2 times 3=38$，$V'=16+25+10=51$，$E'=2 times 25+38=88$。后续面数为 152、608，边界边数为 48、96，所有轮次 $chi=V-E+F=1$，与无孔连通平面区域一致。

#figure(
    table(
        columns: (1.15fr, 1.4fr, 1.2fr),
        inset: 6pt,
        stroke: .5pt + luma(65%),
        table.header([检查对象（原编号）], [解析结果], [意义]),
        [面 $(6,7,11)$], [$(5/3,4/3)$], [三角面点平均],
        [点 1], [$(1/8,1/8)$], [边界角点],
        [点 6 / 点 11], [$(1.06,1.06)$ / $(1.94,1.94)$], [五价点规则],
        [点 7], [$(193/96,95/96)$], [邻接三角形的四价点],
        [边 $(6,7)$ 的边点], [$(37/24,23/24)$], [内部边两面参与],
        [边 $(6,11)$ 的边点], [$(1.5,1.5)$], [共享对角边唯一边点],
    ),
    caption: [首轮结果的独立解析值，测试容差为 $10^(-11)$],
)


== 指定网格的连续迭代图

#figure(image("image/required_overview.png"), caption: [指定网格第 0、1、2、3 轮，从左到右、从上到下排列])

首轮的深色、绿色、橙色点分别代表更新后的旧顶点、边点和面点。两个中心三角形分别生成三个四边形，原对角边被拆为两段，相邻面在同一个边点处连接，没有裂缝。五价点 6、11 周围保持五条相邻边，局部网格因此与规则四价区域不同。

蓝色粗线表示边界，灰色细线是原控制网格。四个角随迭代变圆，内部网格逐渐平滑和加密。所有轮次使用原始包围盒确定的同一比例尺，并把数学坐标的 $Y$ 轴向上转换为图像行坐标向下，因此收缩不是自动缩放造成的视觉错觉。这些图是有限轮次控制网格，并非精确极限曲面的求值图。

== 拓展实例：非均匀坐标网格

`warped.obj` 保持指定样例的连接关系，扰动边界和内部点。例如点 6 改为 $(0.75,1.15)$，点 7 改为 $(2.2,0.8)$，点 10 改为 $(1.1,2.3)$，点 11 改为 $(1.85,1.9)$。它打破等距网格的对称性，可观察算法是否真正使用坐标和邻接信息。

#figure(image("image/warped_overview.png"), caption: [不规则网格第 0 至 3 轮的实际输出])

扰动后的内部折线和外轮廓逐步平滑；局部顶点移动并不相同，但网格连接仍连续。由于拓扑不变，各轮的点、边、面计数与指定样例完全一致。另将该网格施加含非均匀缩放、剪切和平移的仿射变换，验证“先变换再细分”和“先细分再变换”的结果在 $10^(-11)$ 容差内相同。

== 拓展实例：单面与一般多边形

#pair("quad", [单个正方形：第 0 轮与第 3 轮])

单个四边形没有内部旧顶点和内部旧边，首轮四个角全部使用边界公式。边长为 2 时，点 $(0,0)$ 更新为 $(0.25,0.25)$，面点为 $(1,1)$。这组输入用于检查边界处理不依赖内部面存在。

#pair("pentagon", [不规则五边形：第 0 轮与第 3 轮])

五边形首轮产生 5 个四边形，原面点成为五价内部点，下一轮必须继续使用一般价数公式。三轮后共 80 个四边形，轮廓逐渐平滑。

== 拓展实例：三价点与双边界

#pair("fan3", [三个三角形组成的扇形网格：第 0 轮与第 3 轮])

内部点 $P=(0.2,0.1)$ 的三个邻点为 $(2,0)$、$(-1,2)$、$(-1,-2)$，其和为零。由 $overline(F)=P/3$、$overline(R)=P/2$ 得首轮 $P'=4P/9=(4/45,2/45)$。这验证了 $n=3$ 时旧点显式系数 $n-3$ 为零，但邻域均值仍含旧点信息。

#pair("ring", [四个四边形组成的带孔网格：第 0 轮与第 3 轮])

带孔网格有独立的内外边界，孔始终保留，不把内边界误当成需要填充的面。两条轮廓同时变圆，边界边数从 8 增至 64；第 3 轮 $(V,E,F)=(288,544,256)$，各轮 $chi=0$，与一个孔的连通区域一致。

= 实验中存在的问题及解决

- *边界信息缺失：* 通过边的邻接面数识别边界；每个合法边界点必须恰有两条边界边。单面、外轮廓和孔洞均使用同一平滑边界规则。
- *共享边与原地更新：* 无向边统一去重，只计算一次边点；采用新旧网格分离，避免后续顶点读取到本轮已更新位置。重建面、边和一邻域，使迭代可持续进行。
- *显示与复现：* OpenCV 使用 `CV_8UC3` 画布和 BGR 颜色。固定视口、翻转 $Y$ 轴并按唯一边绘制，避免坐标倒置、反复缩放或共享边重复绘制造成误读；统一保存各轮图片和 OBJ。
- *适用范围：* 当前为 XY 平面几何 OBJ 子集，不处理纹理、法线、三维坐标和锐边标签；检查单个面自交与拓扑合法性，但不执行不同面之间的全局相交检测。实验数据保证构成无重叠的平面网格。

完成了数据录入、邻接结构、标准细分规则、连续迭代、网格绘制与读写复现。结果说明几何位置与连接关系必须同时更新；除观察平滑效果，还需要通过解析坐标与拓扑不变量判断正确性。
