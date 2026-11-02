from __future__ import annotations

import re
import sys
from pathlib import Path

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")

ROOT = Path(__file__).resolve().parents[1]
REQUIRED = [
    "README.md",
    "NOTICE.md",
    "实验要求/README.md",
    "实验要求/guide.md",
    "实验要求/setup.md",
    "实验要求/dataset.md",
    "实验要求/technology.md",
    "实验要求/submission.md",
    "实验要求/blender_dataset_guide.md",
    "框架/train_course_baseline.py",
    "框架/requirements.txt",
    "框架/requirements-cpu.txt",
    "框架/local_renderformer/layers/attention.py",
    "框架/local_renderformer/models/renderformer.py",
    "框架/local_renderformer/models/view_transformer.py",
    "paper/renderformer-paper.pdf",
]
LINK_RE = re.compile(r"!?(?:\[[^\]]*\])\(([^)]+)\)")

errors: list[str] = []
for rel in REQUIRED:
    if not (ROOT / rel).exists():
        errors.append(f"缺少必需文件：{rel}")

for md in ROOT.rglob("*.md"):
    text = md.read_text(encoding="utf-8")
    if "pan.ustc.edu.cn" in text:
        errors.append(f"仍包含中科大网盘链接：{md.relative_to(ROOT)}")
    for target in LINK_RE.findall(text):
        if target.startswith(("http://", "https://", "mailto:", "#")):
            continue
        target = target.split("#", 1)[0]
        if not target:
            continue
        if not (md.parent / target).resolve().exists():
            errors.append(f"断开的本地链接：{md.relative_to(ROOT)} -> {target}")

# 课程框架必须保留 TODO，且不能已经出现参考实现。
todo_files = [
    ROOT / "框架/local_renderformer/layers/attention.py",
    ROOT / "框架/local_renderformer/models/renderformer.py",
    ROOT / "框架/local_renderformer/models/view_transformer.py",
]
todo_count = sum(p.read_text(encoding="utf-8").count("HW8_TODO") for p in todo_files)
if todo_count != 14:
    errors.append(f"HW8_TODO 标记数量异常：期望 14（注释和异常各计一次），实际 {todo_count}")

archives = list((ROOT / "dataset").glob("*.zip"))
if len(archives) < 4:
    errors.append(f"数据压缩包数量异常：期望至少 4 个，实际 {len(archives)}")

if errors:
    print("验证失败：")
    for error in errors:
        print(f"- {error}")
    raise SystemExit(1)

print("验证通过")
print(f"必需文件：{len(REQUIRED)} 项")
print(f"本地 Markdown 链接：正常")
print(f"HW8_TODO 标记：{todo_count}（7 个 TODO 位置，每处含注释和异常提示）")
print(f"数据压缩包：{len(archives)} 个")
print("注意：当前环境未安装 PyTorch，训练 smoke test 需要学生安装依赖后运行。")
