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
#let title = "实验2：变换与投影"

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

- 理解模型、视图、投影和视口变换，使用齐次坐标把三维顶点映射到屏幕。
- 逐项构造绕 Z 轴的旋转矩阵和透视投影矩阵，绘制给定三角形的线框。
- 完成提高题：用 Rodrigues 公式实现绕任意过原点轴的旋转。

= 实验环境介绍

- macOS: 26.6.2
- Apple clang: 21.0.0，C++17
- eigen: 3.4.1
- OpenCV: 4.14.0
- xmake: 3.1.1

= 解决问题的主要思路

== 变换顺序与模型矩阵

沿用 example 的三个顶点 $(2, 0, -2)$、$(0, 2, -2)$、$(-2, 0, -2)$，相机位于 $(0, 0, 5)$ 并朝向负 Z 方向。采用齐次列向量，变换顺序为 $p_c = P V M p$，其中 $M$ 为模型矩阵，$V$ 为视图矩阵，$P$ 为投影矩阵。光栅化器随后除以齐次分量 $w$，再执行视口变换。

把输入角度转为弧度 $theta = "angle" dot pi / 180$，绕 Z 轴旋转的矩阵为：

$
    M = mat(
        cos theta, -sin theta, 0, 0;
        sin theta, cos theta, 0, 0;
        0, 0, 1, 0;
        0, 0, 0, 1;
    ).
$

正角度表示从正 Z 轴看向原点时逆时针旋转。视图矩阵沿用示例，对顶点平移 $(-e_x, -e_y, -e_z)$，所以零旋转时三个顶点在相机空间中的深度均为 $z=-7$。

== 透视投影与视口变换

设垂直视场角为 $phi$、宽高比为 $a$，近、远平面的正距离为 $n$、$f$。近裁剪面的上边界为 $t=n tan(phi/2)$，右边界为 $r=a t$，对称视锥的投影矩阵为：

$
    P = mat(
        n/r, 0, 0, 0;
        0, n/t, 0, 0;
        0, 0, -(f+n)/(f-n), -(2f n)/(f-n);
        0, 0, -1, 0;
    ).
$

矩阵使 $w_c=-z$，透视除法后越远的物体投影越小。相机空间的 $z=-n$ 和 $z=-f$ 分别映射到标准化设备坐标的 $-1$ 和 $1$。此处 $n$、$f$ 是正距离，不能直接当作相机空间中的负 Z 坐标。

取 $phi=45 degree$、$a=1$、$n=0.1$、$f=50$。窗口为 $700 times 700$，光栅化器按 $x_s=350(x_("ndc")+1)$、$y_s=350(y_("ndc")+1)$ 映射到屏幕，再用 Bresenham 线段算法绘制三条边。

== 任意轴旋转（提高题）

先将非零轴向量归一化为 $bold(u)=(x,y,z)^T$，再使用 Rodrigues 公式：

$
    R = cos theta I + (1-cos theta) bold(u) bold(u)^T
    + sin theta mat(0, -z, y; z, 0, -x; -y, x, 0).
$

将 $R$ 放入 $4 times 4$ 齐次矩阵的左上角，最后一行和一列补为齐次旋转形式。在 ```cpp get_rotation(axis, angle)``` 中逐项构造矩阵；轴为 $(0,0,1)$ 时，公式退化为上面的 Z 轴旋转。

= 实验步骤与实验结果

== 程序实现

```sh include/Triangle.hpp``` 用三个顶点组成的 ```cpp std::array``` 表示线框三角形；```sh include/rasterizer.hpp``` 声明光栅化接口，```sh src/rasterizer.cpp``` 实现坐标变换和线段绘制。

在 ```sh src/main.cpp``` 中补全矩阵，保留示例的视图矩阵和顶点数据。无参数时进入交互窗口；按 A、D 每次分别增加、减少 $10 degree$，按 Esc 或关闭窗口退出。

#raw(block: true, read("../src/main.cpp"), lang: "cpp")

== 构建与运行

#raw(block: true, read("../xmake.lua"), lang: "lua")

== 结果分析

#figure(
    grid(
        columns: (1fr, 1fr),
        gutter: 10pt,
        figure(image("image/rotate_0.png"), caption: [Z 轴：$0degree$], supplement: none),
        figure(image("image/rotate_20.png"), caption: [Z 轴：$20degree$], supplement: none),

        figure(image("image/rotate_minus20.png"), caption: [Z 轴：$-20degree$], supplement: none),
        figure(image("image/axis_30_1_1_1.png"), caption: [轴 (1, 1, 1)：$30degree$], supplement: none),
    ),
    caption: [程序实际输出的线框三角形],
)

= 实验中存在的问题及解决

- *旋转角度单位：* 命令行以度输入，三角函数使用弧度，因此统一先乘 $pi/180$；计算半视场角时使用 $pi/360$。
- *投影矩阵符号：* 明确相机朝向负 Z，近远平面参数为正距离，设置 $w_c=-z$，并使近远平面映射到 $[-1,1]$。
- *图像行索引：* 屏幕坐标原点在左下角，帧缓冲区从左上角存储，写像素时采用 ```cpp (height - 1 - y) * width + x```，避免底边越界。
- *任意旋转轴：* Rodrigues 公式要求单位向量，先归一化输入，并拒绝零向量，避免产生无效矩阵。
