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
#let title = "实验3：贝塞尔曲线"

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

= 实验目的

- 理解 Bézier 曲线的 控制点、次数、端点插值和凸包性质。
- 实现递归 de Casteljau 算法和基于 Bernstein 多项式的求值算法，分别绘制绿色、红色曲线，并通过黄色叠加验证两者的一致性。
- 从默认的四控制点三次曲线推广到不同数量的控制点，完成交互绘制、文件输入、自动化测试和结果复现。

= 实验环境介绍

- macOS: 26.6.2
- Apple clang: 21.0.0，C++17
- OpenCV: 4.14.0
- xmake: 3.1.1

= 解决问题的主要思路

== de Casteljau 递归算法

给定 $n+1$ 个控制点 $P_0, P_1, dots, P_n$，参数 $t in [0,1]$。先令 $P_i^((0))=P_i$，每轮对相邻点进行线性插值：

$ P_i^((r))(t) = (1-t) P_i^((r-1))(t) + t P_(i+1)^((r-1))(t), quad r=1,dots,n. $

每轮控制点数量减少一个，最终的 $P_0^((n))(t)$ 就是曲线点。函数 ```cpp recursive_bezier``` 检查输入后调用递归辅助函数 ```cpp reduce```；递归终止条件是只剩一个点。四个控制点依次生成三个、两个、一个中间点。

每个参数值需要 $n+(n-1)+dots+1=n(n+1)/2$ 次插值，因此时间复杂度为 $O(n^2)$。每层保存缩短后的向量，递归深度为 $O(n)$，峰值辅助存储为 $O(n^2)$。允许 1 至 64 个控制点，足以覆盖单点、线段、三次曲线及更高次曲线。

== Bernstein 多项式算法

独立使用 Bernstein 基函数进行加权求和：

$ B(t) = sum_(i=0)^n binom(n, i) (1-t)^(n-i) t^i P_i. $

当 $n=3$ 时，有 $B(t)=(1-t)^3P_0+3t(1-t)^2P_1+3t^2(1-t)P_2+t^3P_3$，与示例中的三次多项式一致。实现推广了 ```cpp naive_bezier``` 的绘制过程，由 ```cpp bernstein_bezier``` 求值，不调用递归算法，因此可以作为独立对照。

二项式系数采用 $binom(n, 0)=1$ 和 $binom(n, i+1)=binom(n, i)(n-i)/(i+1)$ 递推，避免直接计算整数阶乘导致溢出。每次求值累加 $n+1$ 项；按幂运算为常数成本计，时间为 $O(n)$，辅助空间为 $O(1)$。两种方法均用双精度坐标，并显式返回 $t=0$ 和 $t=1$ 的端点，减少端点误差。

两种表达在数学上等价。因为各项权重非负且和为 1，曲线位于控制点的凸包中，经过首末控制点，但通常不经过中间控制点。重复控制点属于合法输入；所有点重合时曲线退化为一个点。

== 采样与颜色叠加

窗口为 $700 times 700$，坐标原点位于左上角，$X$ 向右、$Y$ 向下。用整数 $i=0,dots,1000$ 构造 $t=i/1000$，共求值 $1001$ 次，避免 ```cpp t += 0.001``` 的累积误差漏掉最后一个端点。对坐标先检查边界，再用 ```cpp cvRound``` 取最近像素。

OpenCV 图像保持 BGR 顺序。递归曲线只写第 1 通道，多项式曲线只写第 2 通道；叠加时两个通道都保留，像素为 $(B,G,R)=(0,255,255)$，显示为黄色。控制点以白色圆环标出，方便判断曲线端点和形状。

= 实验步骤与实验结果

== 实现

#raw(block: true, read("../lib/bezier.cpp"), lang: "cpp")

#raw(block: true, read("../xmake.lua"), lang: "lua")

== 四控制点绘制结果

采用 ```sh cubic.in``` 中的 $(100,550)$、$(200,100)$、$(500,150)$、$(600,550)$。当 $t=0.5$ 时，权重为 $(1,3,3,1)/8$，解析结果为 $(350,231.25)$，两种实现均通过此坐标断言。

#figure(
    grid(
        columns: (1fr, 1fr, 1fr),
        gutter: 6pt,
        stack(spacing: 4pt, image("image/cubic_casteljau.png"), align(center)[递归算法：绿色]),
        stack(spacing: 4pt, image("image/cubic_bernstein.png"), align(center)[多项式算法：红色]),
        stack(spacing: 4pt, image("image/cubic_both.png"), align(center)[两种算法叠加：黄色]),
    ),
    caption: [相同四控制点下的两种算法对照],
)

曲线通过首末控制点，朝中间两个控制点方向弯曲。测试在没有控制点圆环的画布上统计曲线像素，829 个着色像素全部为黄色，说明该数据下两种算法映射到的像素集合完全一致。

== 不同曲线与边界情况

#figure(
    grid(
        columns: (1fr, 1fr, 1fr),
        gutter: 6pt,
        stack(spacing: 4pt, image("image/s_curve_both.png"), align(center)[四点 S 形曲线]),
        stack(spacing: 4pt, image("image/six_points_both.png"), align(center)[六点五次曲线]),
        stack(spacing: 4pt, image("image/boundary_both.png"), align(center)[边界控制点]),
    ),
    caption: [不同控制点配置的实际叠加输出],
)

S 形数据为 $(100,350)$、$(250,50)$、$(450,650)$、$(600,350)$，中间控制点分处两侧，形成反向弯曲。六点数据为 $(50,550)$、$(150,100)$、$(250,600)$、$(400,100)$、$(550,600)$、$(650,150)$，对应五次曲线；$t=0.5$ 时的权重为 $(1,5,10,10,5,1)/32$，解析坐标为 $(334.375,350)$。

边界数据使用 $(0,0)$、$(699,0)$、$(0,699)$、$(699,699)$，验证图像首末行列能够安全着色。另以两个点验证线性插值，以一个点和四个重合点验证退化曲线。

= 实验中存在的问题及解决

- *递归终止与重复计算：* 每次先求整层相邻插值点，再递归一次，直到只剩一个点；避免分别展开左右子问题造成大量重复求值。
- *多项式推广：* 示例写死了四点三次公式。实现按控制点数量计算次数，用二项式系数递推，使六点和其他数量使用相同接口。
- *颜色覆盖：* 若直接写入整组三通道颜色，后画的绿色会覆盖红色。改为只修改各自通道，保留叠加的黄色，并统一使用 BGR 保存和显示。
- *端点和图像越界：* 用整数生成参数，显式处理端点；写像素前检查坐标范围，防止 700 被用作合法索引。文件中的画布外坐标直接报错，底层绘制接口对画布外采样点进行裁剪。
