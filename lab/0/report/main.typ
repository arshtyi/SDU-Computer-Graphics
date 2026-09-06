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
#let title = "实验0：虚拟机的使用"

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

熟悉图形学实验的开发与编译流程，掌握 Eigen 的向量、矩阵基本运算，并利用齐次坐标实现二维点的旋转和平移。

= 实验环境介绍

- macos: 26.6.2
- clang: 23.1.0
- eigen: 3.4.1
- xmake: 3.1.1 (cmake: 4.4.3)
- typst: 0.15.1

= 解决问题的主要思路

将点 $P = (2, 1)$ 写为齐次列向量 $P_h = (2, 1, 1)^T$，构造绕原点逆时针旋转 $45 degree$ 的矩阵 $R$ 和平移 $(1, 2)$ 的矩阵 $T$：

$
    R = mat(c, -s, 0; s, c, 0; 0, 0, 1), quad
    T = mat(1, 0, 1; 0, 1, 2; 0, 0, 1), quad
    c = s = sqrt(2) / 2.
$

采用列向量时，变换从右向左作用，因此先旋转后平移应计算 $P'_h = T R P_h$。程序使用 ```cpp Eigen::Matrix3d``` 和 ```cpp Eigen::Vector3d``` 完成计算，再将前两项除以齐次分量，得到二维坐标。

= 实验步骤与实验结果

+ 运行 ```sh example/main.cpp```，练习向量加减、数乘、点积和矩阵运算。其中 $v = (1, 2, 3)$、$w = (1, 0, 0)$ 的点积为 $1$，矩阵乘法结果验证了乘法顺序会影响结果。
    #raw(block: true, read("../example/main.cpp"), lang: "cpp")
+ 在 ```sh src/main.cpp``` 中定义点、旋转矩阵和平移矩阵，计算组合变换，并保留六位小数输出。
    #raw(block: true, read("../src/main.cpp"), lang: "cpp")

+ 编写 ```sh xmake.lua``` 并编译执行
    #raw(block: true, read("../xmake.lua"), lang: "lua")
    ```fish
    xmake
    xmake run Example
    xmake run Transformation
    # xmake project -k cmakelists
    ```

理论结果为 $P' = (1 + sqrt(2)/2, 2 + 3 sqrt(2)/2)$，程序输出与之相符：

```text
Cartesian coordinates: (1.707107, 4.121320)
```

= 实验中存在的问题及解决

- *头文件路径：* 本机 Eigen 位于 ```sh /opt/homebrew/include/eigen3```，将其加入包含路径后，使用 ```cpp #include <Eigen/Core>```。
- *角度单位：* ```cpp std::sin```、```cpp std::cos``` 接收弧度，先将 $45 degree$ 转换为 $pi/4$。
- *变换顺序：* 矩阵乘法不可交换，使用 ```cpp translation * rotation``` 保证先旋转、后平移。
