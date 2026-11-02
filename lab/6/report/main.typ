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
#let title = "实验6：RenderFormer 神经渲染"

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

#let rabbit = json("data/rabbit_metrics.json")
#let teapot = json("data/teapot_metrics.json")
#let rabbit4 = json("data/rabbit-p4_metrics.json")
#let teapot4 = json("data/teapot-p4_metrics.json")
#let config8 = json("data/rabbit_config.json")
#let config4 = json("data/rabbit-p4_config.json")
#let decimal(value, digits: 2) = eval(mode: "math", str(calc.round(value, digits: digits)))
#let result-row(name, patch, steps, result) = (
    [#name],
    [#patch],
    [#decimal(steps, digits: 0)],
    [#decimal(result.train.psnr)],
    [#decimal(result.validation.psnr)],
    [#decimal(result.validation.log_l1, digits: 4)],
    [#decimal(result.training_seconds)],
)

= 实验目的

- 理解 RenderFormer 从三角形网格、材质和相机射线直接预测图像的两阶段结构。
- 实现 Triangle Embedding、Ray Bundle Embedding、Encoder/Decoder 的空间 RoPE、多头缩放点积注意力、Encoder Self-Attention 与 Decoder Cross-Attention。
- 记录训练曲线、PSNR、预测图和误差。
- 比较不同射线 patch 大小和训练步数的效果，分析小样本神经渲染的局限。

= 实验环境介绍

- python: 3.13.13
- pytorch: 2.15.0
- mps

= 解决问题的主要思路

== 整体结构与实现范围

先将三角形属性映射为 token，在与视角无关的 Encoder 中交换场景信息；再将每个图像 patch 的相机射线编码为 query，利用 Decoder 对三角形 token 做交叉注意力，最后投影回像素。整个预测过程由可训练的张量运算组成，没有调用光栅化器或路径追踪器替代神经渲染。

保留两阶段注意力、空间 RoPE、可学习的 register/triangle/ray token 和 patch 输出头，使用 $2$ 层 Encoder、$2$ 层 Decoder、$4$ 个头与 $144$ 维隐藏特征。采用 LayerNorm、GELU，以及一致的世界坐标系。法线直接线性投影，射线方向使用 Fourier 编码。

== 七个任务的实现

#table(
    columns: (auto, 1.15fr, 2.7fr),
    inset: 5pt,
    [序号], [任务], [实现与关键处理],
    [$1$],
    [Triangle Embedding],
    [材质 patch 展平并线性投影，与三个顶点的法线投影相加，LayerNorm 后加入可学习 triangle token；在序列前拼接 register tokens。顶点位置通过后续 RoPE 参与计算。],

    [$2$],
    [Ray Bundle Embedding],
    [归一化世界坐标射线做 $4$ 个频率的正弦与余弦编码，按空间顺序组成 #box[$p times p$] patch，再线性投影、归一化并加 ray token。],

    [$3$],
    [Encoder RoPE],
    [三个顶点的 $9$ 个坐标各乘 $2$ 个频率，为每个头生成 $18$ 个旋转角，旋转 $36$ 个特征。register 使用有效三角形顶点的中心位置。],

    [$4$],
    [Decoder RoPE],
    [query 使用重复三次的相机原点（$9$ 维），key 使用三角形顶点（$9$ 维）；频率与坐标系保持一致。],

    [$5$],
    [Scaled Dot-Product Attention],
    [显式实现缩放点积、有效 key mask、softmax 与 value 加权，不把任务核心交给内置 SDPA。],

    [$6$],
    [Encoder Self-Attention],
    [三角形和 register 共同作为 $Q$、$K$、$V$；逐层执行 pre-norm 注意力、残差和 FFN，支持从 $1$ 开始计数的跨层 skip。],

    [$7$],
    [Decoder Cross-Attention],
    [射线 query 读取 Encoder 上下文，随后执行射线 Self-Attention 与 FFN；输出头用 softplus 得到非负、无上界的 HDR RGB。],
)

== 张量形状与 patch 顺序

设批量大小为 $B$、三角形数上界为 $N$、register 数为 $R=4$、隐藏维度为 $D=144$，每头维度为 $d=36$。

#table(
    columns: (1fr, 1fr),
    inset: 8pt,
    [张量], [形状],
    [三角形位置 / 顶点法线], [$B times N times 9$],
    [材质 patch], [$B times N times 10 times P times P$，本次 $P=1$],
    [Encoder token], [$B times (N+R) times D$],
    [射线方向], [$B times H times W times 3$],
    [Decoder query], [$B times (H/p)(W/p) times D$],
    [$Q, K, V$], [$B times 4 times N_"token" times d$],
    [输出], [$B times 3 times H times W$，线性 HDR RGB],
)

patch 排列先按行列分块，再把块内的行、列和特征展平。输出按逆顺序还原，保留矩形图像支持；图像高宽必须能被 patch 大小整除。$64 times 64$ 图像在 $p=8$ 时产生 $64$ 个 query，在 $p=4$ 时产生 $256$ 个 query。前者计算更少，后者保留更细的局部信息，但射线自注意力的矩阵面积变为 $16$ 倍。

== 空间编码、mask 与残差

RoPE 对相邻特征对使用

$
    R(theta) vec(x_1, x_2)
    = vec(x_1 cos theta - x_2 sin theta, x_1 sin theta + x_2 cos theta).
$

每个三角形坐标乘以频率 $1$ 和 $2$；query 与 key 的旋转点积依赖两者位置之差。所有三角形和相机同时平移时，这个差保持不变。坐标与中心位置均只由有效三角形计算，不能让 padding 改变 register 的位置。位置旋转因子每次 forward 计算一次，并在各层复用。

注意力定义为

$
    op("Attention")(Q, K, V)
    = op("softmax")((Q K^upright(T)) / sqrt(d) + M) V.
$

其中 $M$ 为加性 mask：有效 key 对应 $M_(i j)=0$，无效 key 对应 $M_(i j)=-oo$。实现约定 `True` 表示有效，先把无效 key/value 置零，再屏蔽分数。全无效 key 的权重返回 $0$，避免全为 $-oo$ 的 softmax 产生 NaN；模型中的正数 register 也保证上下文始终有有效 token。测试还使用 NaN padding，确认被屏蔽的内容不会污染输出。

每层采用 pre-norm 残差结构 $x' = x + op("Attention")(op("LN")(x))$ 与 $y = x' + op("FFN")(op("LN")(x'))$。额外 skip 的层号从 $1$ 开始，在源层之前保存，在目标层之后相加，并验证源层严格早于目标层。正式训练不启用额外 skip；该分支通过前向、反向与非法配置测试。

== HDR 数据与评价指标

使用 softplus 输出 HDR。设 $I_i$ 与 $hat(I)_i$ 为展平后的 GT 和预测像素值，最小化对数域 $L_1$ 损失：

$
    cal(L) = 1/(3 B H W) sum_(i=1)^(3 B H W)
    abs(ln(1 + hat(I)_i) - ln(1 + I_i)).
$

材质实际包含 $13$ 个通道：前 $7$ 个为 diffuse、specular 和 roughness，中间 $3$ 个为 normal map，最后 $3$ 个为 emission。读取时选择前 $7$ 个和最后 $3$ 个，emission 做 `log1p`；材质 patch 采用平均池化到 $1 times 1$，而不是把 normal map 当作发光或随意截断通道。

展示图和 PSNR 统一使用固定 Reinhard 映射 $T(I)=I/(1+I)$。设验证图像数为 $n_"val"$，索引 $i$ 遍历全部 RGB 像素，以峰值 $1$ 计算 PSNR：

$
    upright("MSE") = 1/(3 n_"val" H W) sum_i (T(hat(I)_i) - T(I_i))^2,
    quad upright("PSNR") = -10 log_10(upright("MSE")).
$

= 实验步骤与实验结果

== 训练参数与定量结果

随机种子 $42$；AdamW；学习率 $0.001$；weight decay $0.01$；batch size $1$；前 $50$ 步 warmup，随后余弦衰减到初始学习率的 $10%$；梯度范数截断为 $1$。基线使用 $8 times 8$ patch、$1200$ 步，模型参数量为 $#config8.parameters$；补充实验使用 $4 times 4$ patch、$4000$ 步，参数量为 $#config4.parameters$。

#table(
    columns: (1fr,) * 7,
    inset: 4pt,
    [场景],
    [patch],
    [步数],
    [训练 PSNR $upright("dB")$],
    [验证 PSNR $upright("dB")$],
    [验证log-$L_1$],
    [训练时间 $upright(s)$],
    ..result-row("Rabbit", $8 times 8$, 1200, rabbit),
    ..result-row("Teapot", $8 times 8$, 1200, teapot),
    ..result-row("Rabbit", $4 times 4$, 4000, rabbit4),
    ..result-row("Teapot", $4 times 4$, 4000, teapot4),
)

== 训练曲线

#figure(image("image/rabbit_curves.png"), caption: [Rabbit 基线：log-$L_1$ 与验证 PSNR。])
#figure(image("image/teapot_curves.png"), caption: [Teapot 基线：log-$L_1$ 与验证 PSNR。])
#figure(image("image/rabbit-p4_curves.png"), caption: [Rabbit：$4 times 4$ patch、$4000$ 步。])
#figure(image("image/teapot-p4_curves.png"), caption: [Teapot：$4 times 4$ patch、$4000$ 步。])

== 留出视角预测与误差

每行从左到右为 GT、预测和 $5$ 倍绝对误差；GT 与预测均先使用同一个固定 Reinhard 映射。

#figure(
    image("image/rabbit-p4_comparison.png", height: 90%),
    caption: [Rabbit 细 patch 配置：全部留出视角的 GT、预测与误差。],
)
#figure(
    image("image/teapot-p4_comparison.png", height: 90%),
    caption: [Teapot 细 patch 配置：全部留出视角的 GT、预测与误差。],
)

基线预测主要恢复了房间的大块红绿颜色和相机视角变化，物体细节、高光及遮挡边界仍明显模糊。细 patch 配置使 Rabbit 验证 PSNR 从 $#decimal(rabbit.validation.psnr)$ 提高到 $#decimal(rabbit4.validation.psnr) thin upright("dB")$，使 Teapot 从 $#decimal(teapot.validation.psnr)$ 提高到 $#decimal(teapot4.validation.psnr) thin upright("dB")$；Teapot 轮廓与局部颜色更容易辨认，但 Rabbit 表面和两组图像中的亮光源仍未准确恢复。
