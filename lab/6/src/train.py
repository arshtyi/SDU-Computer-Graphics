from __future__ import annotations

import csv
import json
import math
import platform
import random
import time
from dataclasses import asdict
from pathlib import Path
from typing import Any, TypedDict

import numpy as np
import torch
import torch.nn.functional as F
from PIL import Image
from torch import Tensor

from data import SceneDataset
from model import ModelConfig, RenderFormer


class ViewMetrics(TypedDict):
    view: str
    mse: float
    psnr: float
    log_l1: float


def pick_device(name: str) -> torch.device:
    if name == "auto":
        name = "cuda" if torch.cuda.is_available() else "mps" if torch.backends.mps.is_available() else "cpu"
    return torch.device(name)


def synchronize(device: torch.device) -> None:
    if device.type == "mps":
        torch.mps.synchronize()
    elif device.type == "cuda":
        torch.cuda.synchronize()


def tone_map(image: Tensor) -> Tensor:
    return image / (1 + image)


def psnr(mse: float) -> float:
    return -10 * math.log10(max(mse, 1e-12))


def save_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def save_image(path: Path, image: Tensor) -> None:
    array = image.detach().cpu().clamp(0, 1).permute(1, 2, 0).numpy()
    Image.fromarray(np.round(array * 255).astype(np.uint8)).save(path)


@torch.no_grad()
def measure(
    model: RenderFormer,
    dataset: SceneDataset,
    indices: list[int],
    device: torch.device,
    output: Path | None = None,
) -> dict[str, Any]:
    model.eval()
    rows: list[ViewMetrics] = []
    if output:
        output.mkdir(parents=True, exist_ok=True)
    start = time.perf_counter()
    for index in indices:
        batch = dataset.batch([index], device)
        prediction = model(batch)
        target = batch["target"]
        pred_ldr, gt_ldr = tone_map(prediction), tone_map(target)
        mse = F.mse_loss(pred_ldr, gt_ldr).item()
        rows.append(
            {
                "view": dataset.names[index],
                "mse": mse,
                "psnr": psnr(mse),
                "log_l1": F.l1_loss(prediction.log1p(), target.log1p()).item(),
            }
        )
        if output:
            stem = f"view_{index:04d}"
            save_image(output / f"{stem}_gt.png", gt_ldr[0])
            save_image(output / f"{stem}_pred.png", pred_ldr[0])
            save_image(output / f"{stem}_error.png", 5 * (pred_ldr[0] - gt_ldr[0]).abs())
    synchronize(device)
    return {
        "psnr": psnr(sum(row["mse"] for row in rows) / len(rows)),
        "mean_view_psnr": sum(row["psnr"] for row in rows) / len(rows),
        "log_l1": sum(row["log_l1"] for row in rows) / len(rows),
        "seconds": time.perf_counter() - start,
        "views": rows,
    }


def plot_curves(rows: list[dict[str, Any]], output: Path) -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, axes = plt.subplots(1, 2, figsize=(9, 3.1), constrained_layout=True)
    axes[0].plot([r["step"] for r in rows], [r["train_log_l1"] for r in rows], label="Train batch")
    axes[0].plot([r["step"] for r in rows], [r["val_log_l1"] for r in rows], label="Validation")
    axes[0].set(xlabel="Step", ylabel="log(1+HDR) L1")
    axes[0].legend()
    axes[1].plot([r["step"] for r in rows], [r["val_psnr"] for r in rows])
    axes[1].axhline(15, color="gray", linestyle="--", label="15 dB")
    axes[1].set(xlabel="Step", ylabel="Validation PSNR (Reinhard, dB)")
    axes[1].legend()
    for axis in axes:
        axis.grid(alpha=0.2)
    fig.savefig(output, dpi=160)
    plt.close(fig)


def train(
    dataset: SceneDataset,
    config: ModelConfig,
    output: Path,
    *,
    steps: int,
    batch_size: int,
    learning_rate: float,
    seed: int,
    device: torch.device,
    log_every: int,
) -> dict[str, Any]:
    if steps < 1 or batch_size < 1 or log_every < 1 or learning_rate <= 0:
        raise ValueError("Steps, batch size, logging interval and learning rate must be positive")
    if output.exists() and any(output.iterdir()):
        raise FileExistsError(f"Output directory is not empty: {output}")
    output.mkdir(parents=True, exist_ok=True)
    random.seed(seed)
    np.random.seed(seed)
    torch.manual_seed(seed)
    if device.type == "cuda":
        torch.cuda.manual_seed_all(seed)
    model = RenderFormer(config).to(device)
    optimizer = torch.optim.AdamW(model.parameters(), lr=learning_rate, weight_decay=0.01)
    generator = random.Random(seed)
    settings = {
        "model": asdict(config),
        "steps": steps,
        "batch_size": batch_size,
        "learning_rate": learning_rate,
        "seed": seed,
        "device": str(device),
        "torch": torch.__version__,
        "python": platform.python_version(),
        "platform": platform.platform(),
        "parameters": sum(p.numel() for p in model.parameters()),
        "metric": "PSNR of fixed Reinhard I/(1+I), peak=1, aggregate pixel MSE",
        "dataset": dataset.metadata(),
    }
    save_json(output / "config.json", settings)
    initial = measure(model, dataset, dataset.val_indices, device)
    initial_train = measure(model, dataset, dataset.train_indices, device)
    history = [
        {
            "step": 0,
            "train_log_l1": initial_train["log_l1"],
            "val_log_l1": initial["log_l1"],
            "val_psnr": initial["psnr"],
            "seconds": 0.0,
        }
    ]
    start = time.perf_counter()
    with (output / "history.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(history[0]))
        writer.writeheader()
        writer.writerow(history[0])
        for step in range(1, steps + 1):
            model.train()
            indices = generator.choices(dataset.train_indices, k=batch_size)
            batch = dataset.batch(indices, device)
            warmup = min(50, max(1, steps // 10))
            progress = max(0, step - warmup) / max(1, steps - warmup)
            rate = learning_rate * min(1, step / warmup) * (0.1 + 0.9 * (1 + math.cos(math.pi * progress)) / 2)
            for group in optimizer.param_groups:
                group["lr"] = rate
            optimizer.zero_grad(set_to_none=True)
            prediction = model(batch)
            loss = F.l1_loss(prediction.log1p(), batch["target"].log1p())
            if not torch.isfinite(loss).item():
                raise FloatingPointError(f"Non-finite loss at step {step}")
            loss.backward()
            torch.nn.utils.clip_grad_norm_(model.parameters(), max_norm=1.0, error_if_nonfinite=True)
            optimizer.step()
            if step == 1 or step % log_every == 0 or step == steps:
                validation = measure(model, dataset, dataset.val_indices, device)
                row = {
                    "step": step,
                    "train_log_l1": loss.item(),
                    "val_log_l1": validation["log_l1"],
                    "val_psnr": validation["psnr"],
                    "seconds": time.perf_counter() - start,
                }
                history.append(row)
                writer.writerow(row)
                stream.flush()
                print(
                    f"step={step:5d} loss={loss.item():.5f} val_psnr={validation['psnr']:.3f} elapsed={row['seconds']:.1f}s",
                    flush=True,
                )
    synchronize(device)
    training_seconds = time.perf_counter() - start
    checkpoint = {
        "config": asdict(config),
        "state_dict": model.state_dict(),
        "dataset": dataset.metadata(),
        "step": steps,
    }
    torch.save(checkpoint, output / "final.pt")
    metrics = {
        "initial_validation": initial,
        "train": measure(model, dataset, dataset.train_indices, device),
        "validation": measure(model, dataset, dataset.val_indices, device, output / "image"),
        "training_seconds": training_seconds,
    }
    if device.type == "cuda":
        metrics["peak_allocated_bytes"] = torch.cuda.max_memory_allocated(device)
    elif device.type == "mps":
        metrics["final_allocated_bytes"] = torch.mps.current_allocated_memory()
        metrics["final_driver_allocated_bytes"] = torch.mps.driver_allocated_memory()
    save_json(output / "metrics.json", metrics)
    plot_curves(history, output / "curves.png")
    print(
        json.dumps(
            {
                "train_psnr": metrics["train"]["psnr"],
                "validation_psnr": metrics["validation"]["psnr"],
                "output": str(output),
            },
            ensure_ascii=False,
        ),
        flush=True,
    )
    return metrics


def evaluate(checkpoint: Path, dataset: SceneDataset, device: torch.device, output: Path) -> None:
    saved = torch.load(checkpoint, map_location="cpu", weights_only=True)
    if saved["dataset"] != dataset.metadata():
        raise ValueError("Dataset content, view split or preprocessing differs from the checkpoint")
    model = RenderFormer(ModelConfig(**saved["config"])).to(device)
    model.load_state_dict(saved["state_dict"])
    metrics = measure(model, dataset, dataset.val_indices, device, output / "image")
    save_json(output / "metrics.json", metrics)
    print(json.dumps(metrics, ensure_ascii=False, indent=2))
