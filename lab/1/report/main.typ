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
#let title = "实验1：两种平面三角形点定位算法"

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

- 实现射线求交法、叉积同侧法以及的重心坐标法，判断平面上的点位于三角形内部、边界还是外部
- 理解三种算法的几何原理，并比较其实现特点。

= 实验环境介绍

- macOS: 26.6.2
- clang: 23.1.1，c++17
- eigen: 3.4.1
- xmake: 3.1.1（cmake: 4.4.3）
- typst: 0.15.1

= 解决问题的主要思路

用 ```cpp Eigen::Vector2d``` 表示二维点，```cpp std::array<Point, 3>``` 保存三角形顶点 $A, B, C$。定义二维叉积 $a times b = a_x b_y - a_y b_x$，其正负表示向量的相对方向。

== 公共处理

若 $abs((B-A) times (C-A)) <= epsilon$，认为三角形退化，输出提示并结束程序，避免重心坐标计算中除以零。取 $epsilon = 10^(-9)$，用于常规数量级坐标下的浮点比较。

对于每条边 $A B$，若 $abs((B-A) times (P-A)) <= epsilon$ 且 $(P-A) dot (P-B) <= epsilon$，则点在线段上，统一输出 ```text boundary```。第二个条件用于排除边的延长线上的点。其余点分别调用三种算法，输出 ```text inside``` 或 ```text outside```。

== 射线求交法

从 $P$ 向水平方向右侧作射线。对每条边 $A B$，仅当 $A_y > P_y$ 与 $B_y > P_y$ 的真假不同，才计算交点横坐标：

$ x = A_x + (P_y-A_y)(B_x-A_x)/(B_y-A_y). $

若 $x > P_x$，交点数加一。交点数为奇数时在内部，为偶数时在外部。上述半开区间条件自动跳过水平边；射线通过顶点时，跨越射线的两条邻边只计一次，在局部极值顶点处则计零次或两次，保持奇偶性正确。

== 叉积同侧法

计算三条有向边与待测点的叉积：

$
    c_1 = (B-A) times (P-A), quad
    c_2 = (C-B) times (P-B), quad
    c_3 = (A-C) times (P-C).
$

排除边界后，三个值全为正或全为负，说明点位于三条边的同一内侧，即三角形内部；否则在外部。同时接受两种符号，因而不要求顶点按逆时针顺序输入。

== 重心坐标法

将待测点写成 $P = (1-u-v) A + u B + v C$。令 $D = (B-A) times (C-A)$，由叉积得到：

$ u = ((P-A) times (C-A))/D, quad v = ((B-A) times (P-A))/D. $

排除边界后，当 $u > 0$、$v > 0$ 且 $u+v < 1$ 时，三个权重均为正，点在内部；否则在外部。例如 $A=(0,0)$、$B=(4,0)$、$C=(0,3)$、$P=(1,1)$ 时，$u=1/4$、$v=1/3$、$1-u-v=5/12$，因此点在内部。

三种方法对三角形的时间、额外空间复杂度均为 $Omicron(1)$。射线法可推广到简单多边形；叉积同侧法适用于凸多边形且不需要除法；重心坐标法还可用于三角形内的属性插值。

= 实验步骤与实验结果

== 程序与构建

从输入文件或标准输入读取三个顶点、待测点数量及各点坐标，依次输出三种算法的结果。

#raw(block: true, read("../src/main.cpp"), lang: "cpp")

#raw(block: true, read("../xmake.lua"), lang: "lua")

```sh
xmake
xmake test
xmake run PointLocation
# xmake project -k cmakelists
```

= 实验中存在的问题及解决

- *边界点的归属：* 先使用叉积和点积判断点是否在线段上，将边和顶点统一归为 ```text boundary```，再判断内部与外部。
- *射线经过顶点或水平边：* 用端点纵坐标的半开区间条件决定是否求交，避免重复计数影响奇偶性，同时避免除以零。
- *顶点顺序：* 同侧法同时检查全正和全负，重心坐标保留分母的符号，保证顺、逆时针输入均正确。
- *浮点误差与退化输入：* 使用容差判断近似共线，在计算重心坐标前拒绝退化三角形。
