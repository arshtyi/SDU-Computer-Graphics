# RenderFormer 代码框架

这是实验要求中的待补全代码框架，代码来自课程仓库 `FrameworkRenderformer`，并保留了 `HW8_TODO`。

## 主要入口

```text
train_course_baseline.py                  # 训练入口
baseline_data.py                          # PT 数据读取和 batch 整理
baseline_model.py                         # 模型封装
baseline_loss.py                          # 损失函数
build_pt_dataset_from_blender.py          # 可选：Blender 数据转 PT
local_renderformer/
├── encodings/                             # NeRF、RoPE
├── layers/                                # Attention、DPT
└── models/                                # RenderFormer、ViewTransformer、配置
```

请在代码目录内运行训练脚本，并使用相对于本目录的 `--data_path`。例如：

```powershell
python train_course_baseline.py `
  --dataset_format pt `
  --data_path ..\dataset\pt_dataset_rabbit_v2 `
  --out_dir runs\smoke_test `
  --max_steps 20 `
  --max_items 4 `
  --workers 0 `
  --device cuda
```

提交时删除 `runs/`、checkpoint 和其他训练输出，只保留完整源码。


## 相关文档

- [实验总览](../实验要求/README.md)
- [实现指南](../实验要求/guide.md)
- [环境配置与训练](../实验要求/setup.md)
- [提交说明](../实验要求/submission.md)
