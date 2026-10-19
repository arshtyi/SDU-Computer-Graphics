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
#let title = "实验5：现代图形技术"

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

#let panel(path, label) = stack(spacing: 5pt, image("image/" + path, width: 100%), align(center, text(
    size: 9pt,
    label,
)))
#let comparison(a, b, left, right, caption) = figure(
    grid(
        columns: (1fr, 1fr),
        gutter: 8pt,
        panel(a, left), panel(b, right),
    ),
    caption: caption,
)

= 实验目的

- 补全软件光栅化流水线：投影、覆盖判定、深度测试，以及颜色、法向量、纹理坐标、着色位置的插值。
- 实现法向量显示、Blinn-Phong、纹理映射、凹凸映射与置换映射，区分改变着色法线和改变几何形状的效果。
- 附加：
    - 加载其他带顶点法向量的 OBJ 模型；
    - 实现双线性纹理采样，并与最近邻采样比较。

= 实验环境介绍

- macOS: 26.6.2
- Apple clang: 21.0.0，C++17
- OpenCV: 4.14.0
- xmake: 3.1.1

= 解决问题的主要思路

== 投影、裁剪与像素覆盖

采用右手相机坐标系，相机朝 $-Z$ 方向观察，近平面距离 $n>0$、远平面距离 $f>n$。令 $q=cot(theta/2)$、宽高比为 $a$，透视矩阵为

$ P = mat(q/a, 0, 0, 0; 0, q, 0, 0; 0, 0, -(f+n)/(f-n), -(2f n)/(f-n); 0, 0, -1, 0). $

使用 $theta=45 degree$、$a=1$、$n=0.1$、$f=50$。有 $w_c=-z_v>0$，近平面映至 NDC 的 $-1$，远平面映至 $1$。透视除法前，对 $-w_c <= x_c,y_c,z_c <= w_c$ 六个平面执行 Sutherland-Hodgman 裁剪；新顶点的属性沿裁剪边线性插值，裁剪后的多边形再扇形三角化。

屏幕坐标为 $x_s=W(x_"ndc"+1)/2$、$y_s=H(y_"ndc"+1)/2$。在 $(x+0.5,y+0.5)$ 采样，使用边函数与半开边界规则，使共享边只归属于一个三角形。覆盖计算量化到 $1/256$ 像素并用 64 位整数完成，避免浮点边界误差产生缝隙。包围盒限制在画布内，退化三角形跳过；图像行号是 $H-1-y$。

== 属性插值与深度的区别

设屏幕重心坐标为 $lambda_i$，保存的齐次分量为 $w_i$。颜色、法向量、UV、视空间位置等属性 $A$ 使用透视正确插值：

$ omega_i = (lambda_i/w_i)/(sum_(j=0)^2 lambda_j/w_j), quad A = sum_(i=0)^2 omega_i A_i. $

法向量先经模型视图线性部分的逆转置变换，再插值并归一化。若直接用屏幕重心坐标线性插值 UV，深度变化较大的三角形会发生纹理扭曲。

深度缓冲使用已透视除法的 NDC 深度，必须在屏幕上直接线性插值：

$ d = 1/2 (sum_(i=0)^2 lambda_i z_(c,i)/w_i + 1). $

此处不再次除以 $w_i$ 或乘以透视修正分母。缓冲初始为正无穷，较小深度通过测试。

== Blinn-Phong 与坐标空间

所有光照量统一在视空间：着色点为 $p$，眼睛为原点，世界空间的两个点光源先经视图矩阵变换。材质系数 $k_a=(0.005,0.005,0.005)$、$k_s=(0.7937,0.7937,0.7937)$，镜面指数为 150，环境强度为 $(10,10,10)$。普通着色的 $k_d=(148,121,92)/255$。

$ l_j = (L_j-p)/norm(L_j-p), quad v = -p/norm(p), quad h_j = (l_j+v)/norm(l_j+v). $

$
    C = k_a dot.op I_a + sum_j I_j/(norm(L_j-p)^2) dot.op
    (k_d max(0, n dot l_j) + k_s max(0, n dot h_j)^150).
$

其中 $dot.op$ 表示逐通道相乘。当 $n dot l_j <= 0$ 时不计算该光源的漫反射和镜面项，避免背面高光；退化半程向量与零光源距离单独处理。环境光仅加一次，不能因光源数量增加而重复累加。最终乘 255，并在写入帧缓冲时夹到 $[0,255]$。

== 纹理采样

OpenCV 读图为 BGR，`Texture` 内部转为 RGB。将 UV 夹到 $[0,1]$，映射到 $x=u(W_t-1)$、$y=(1-v)(H_t-1)$，因此 $u=1$、$v=0$ 仍落在合法边界像素。最近邻取距离最近的纹素，双线性取四个邻居：

$ c(x,y)=(1-t)((1-s)c_(00)+s c_(10))+t((1-s)c_(01)+s c_(11)), $

其中 $s=x-floor(x)$、$t=y-floor(y)$，右/下邻居在边缘夹取。`getColorBilinear()` 返回浮点 RGB，片元着色器调用该采样接口，将颜色除以 255 后用作 $k_d$。两种采样共用光照和渲染参数。单行、单列和 $1 times 1$ 图像也能安全采样。

== 凹凸映射：使用 UV 对应的切向量

高度取 RGB 的亮度加权值并归一化到 $[0,1]$：$h=(0.2126R+0.7152G+0.0722B)/255$。这样高度尺度不依赖颜色值使用 0–1 还是 0–255。默认置换距离系数 $k=0.1$，单位为视空间长度。

对每个三角形，由位置差 $e_1,e_2$ 和 UV 差 $(a_u,a_v)$、$(b_u,b_v)$ 求表面偏导：

$ D=a_u b_v-a_v b_u, quad p_u=(b_v e_1-a_v e_2)/D, quad p_v=(-b_u e_1+a_u e_2)/D. $

在片元处把两者投影到插值法向量的切平面，得到 $t_u,t_v$。按一个纹素的 UV 步长求中心差分 $h_u,h_v$，纹理边界使用有效区间的单边差分。扰动法向量为

$ n' = op("normalize")(sigma (t_u+k h_u n) times (t_v+k h_v n)), $

其中 $sigma$ 由 $(t_u times t_v) dot n$ 的符号决定，保证镜像 UV 不反转法线。UV 退化或切向量共线时使用稳定的正交基。常量高度图给出 $n'=n$。

直接从 UV 推导切向量。上述法线近似忽略 $h$ 与原法线偏导相乘的项，适合小尺度凹凸，不是精确的曲面微分求值。

== 置换映射：片元近似与几何置换

`displacement_fragment_shader()` 按实现 $p'=p+k h(u,v)n$，再用 $p'$ 与 $n'$ 计算光照。命令行模式 `displacement-fragment` 用于保留该近似。因为执行时覆盖与深度测试已完成，它不会改变实际轮廓，也不会改变深度缓冲。

正式的 `displacement` 模式先把每个三角形按边中点细分两轮，插值 UV 和法线，三角形数量乘 16；在顶点着色阶段沿视空间法线移动位置，再投影、裁剪和光栅化。因此新的位置实际参与轮廓和深度计算。片元阶段只扰动法线并计算光照，避免重复移动位置。`--subdivide 0..3` 可控制几何细度；固定细分仍会漏掉高度图中的高频细节，细分后的折线曲面不是连续置换曲面的精确解。

= 实验步骤与实验结果

== 构建与运行

#raw(read("../xmake.lua"), lang: "lua", block: true)

```sh
xmake
xmake test
xmake run
```

== 法向量、Blinn-Phong 与纹理

#comparison("spot_normal.png", "spot_phong.png", [normal], [phong], [同一模型与视角下的法向量可视化和 Blinn-Phong 光照])

法向量颜色随表面朝向平滑变化，腿部与头部的前后关系由深度测试决定。Phong 图使用固定的棕色漫反射材质，头部和背部可见镜面高光，背光处仍保留环境光亮度。两张图的几何覆盖完全相同。

#figure(image("image/spot_texture.png"), caption: [Spot 的彩色纹理映射；使用原始 1024 × 1024 纹理与双线性采样])

纹理提供眼睛、鼻部和身体斑块等颜色差异，同时保留点光源产生的明暗与高光。UV 的纵向翻转只在纹理坐标转换中执行一次，防止眼睛与斑块上下颠倒。

== 凹凸与置换

#comparison("spot_bump.png", "spot_bump-phong.png", [扰动法线], [扰动法线参与光照], [相同高度图下的凹凸法线与凹凸着色])

凹凸图反映高度梯度方向，不直接以高度值替换颜色。相较普通 Phong，表面增加细碎的明暗变化；轮廓没有变化，因为只修改着色法线。

#comparison(
    "spot_displacement-fragment.png",
    "spot_displacement.png",
    [片元位置近似],
    [顶点几何置换，细分两轮],
    [片元近似与实际几何置换],
)

#comparison(
    "spot_phong_mask.png",
    "spot_displacement_mask.png",
    [原始几何覆盖],
    [置换后的几何覆盖],
    [由深度缓冲生成的覆盖掩码；白色表示被三角形覆盖],
)

两种置换共享高度尺度。片元近似的覆盖计数与普通 Phong 相同；真正置换会改变外轮廓，且遮挡测试使用新的深度。

#figure(
    table(
        columns: (2fr, 1fr, 1fr),
        inset: 5pt,
        align: center,
        stroke: .5pt + luma(65%),
        table.header([模式], [输入三角形], [覆盖像素]),
        ..csv("data/render_stats.csv").slice(1).flatten(),
    ),
    caption: [700 × 700 输出下的实测几何统计],
)

== 附加题一：更多模型

使用Rock 和 Cube。Rock 包含 98 个位置、98 个顶点法向量、165 个 UV 与 192 个三角面；Cube 包含 8 个位置、8 条法向量记录、14 个 UV 与 12 个三角面。模型沿最长包围盒轴归一化到长度 2 后居中，采用相同的相机和光照。

#comparison("rock_normal.png", "rock_texture.png", [Rock 法向量], [Rock 纹理], [Rock：不规则表面上的法线与纹理])
#comparison("cube_normal.png", "cube_texture.png", [Cube 法向量], [Cube 纹理], [Cube：框架模型原有的硬边法线与 UV])

Cube 的面角分别引用对应面的常量法线，因此保留硬边；相同空间顶点在不同面上可有不同的法线索引。为使完整轮廓落在画布内，Cube 缩放为 1.8，Rock 保持 2.5。Rock 使用 `rock.png`，Cube 使用材质文件指定的 `wall1.tif`。另以 Crate 的六个四边面检查加载器索引，三角化后得到 12 个三角面。

== 附加题二：双线性纹理插值

用系统图像工具把 Spot 颜色纹理缩为 $32 times 32$，不修改原图。最近邻与双线性两次渲染使用同一份小纹理和相同的几何、光照、角度，唯一变化是 `--filter`。

#comparison(
    "small_nearest.png",
    "small_bilinear.png",
    [32 × 32，最近邻],
    [32 × 32，双线性],
    [小纹理放大到模型表面后的采样对照],
)
#comparison(
    "small_nearest_crop.png",
    "small_bilinear_crop.png",
    [最近邻局部],
    [双线性局部],
    [从相同图像位置裁剪的细节；未额外缩放或修改像素],
)

最近邻在低分辨率斑块和眼睛边缘呈现明显的纹素台阶；双线性通过邻近四个纹素的加权值平滑过渡，代价是边缘变软。它不能恢复 32 × 32 纹理在缩小时丢失的细节，也不能解决纹理缩小时的全部走样问题；后者通常还需要 mipmap 或更完整的滤波。

核心采样实现如 `lib/Texture.cpp`：

#raw(
    block: true,
    "Vec3 Texture::getColorBilinear"
        + read("../lib/Texture.cpp").split("Vec3 Texture::getColorBilinear").at(1).split("Vec3 Texture::sample").at(0),
    lang: "cpp",
)

= 实验中存在的问题及解决

- *深度与一般属性混淆：* 保存原始齐次 $w$，普通属性用透视权重，NDC 深度用屏幕重心坐标，交叉遮挡测试验证顺序无关。
- *纹理越界与通道颠倒：* 使用 $W_t-1$、$H_t-1$ 映射和夹取边界，明确 RGB/BGR 转换位置，使用四色小纹理验证。
- *法线显示丢失负分量：* 使用 $[-1,1]$ 到 $[0,255]$ 的线性映射，确保凹凸法线的三个分量都可见。
