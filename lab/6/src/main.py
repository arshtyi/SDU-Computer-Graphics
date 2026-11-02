from __future__ import annotations

import argparse
from pathlib import Path

import torch

from data import COURSE_DATASETS, SceneDataset, course_dataset, download, extract_pt
from model import ModelConfig
from train import evaluate, pick_device, train


def main() -> None:
    parser = argparse.ArgumentParser(description="Lab 6: RenderFormer neural rendering")
    commands = parser.add_subparsers(dest="command", required=True)
    acquire = commands.add_parser("data", help="Download a verified dataset to the user cache")
    source = acquire.add_mutually_exclusive_group()
    source.add_argument("--scene", choices=COURSE_DATASETS, help="Course dataset (default: rabbit)")
    source.add_argument("--url", help="HTTPS URL to a PT ZIP archive")
    source.add_argument("--archive", type=Path, help="Import an existing course PT ZIP to the cache")
    acquire.add_argument("--sha256", help="Required expected SHA-256 for network downloads")
    for name in ("train", "evaluate"):
        command = commands.add_parser(name)
        dataset_source = command.add_mutually_exclusive_group()
        dataset_source.add_argument("--data", type=Path, help="Extracted PT dataset directory")
        dataset_source.add_argument("--scene", choices=COURSE_DATASETS, default="rabbit")
        command.add_argument("--output", type=Path, required=True)
        command.add_argument("--device", choices=("auto", "cpu", "mps", "cuda"), default="auto")
        command.add_argument("--size", type=int, default=64)
        command.add_argument("--texture-size", type=int, default=1)
        command.add_argument("--threads", type=int, default=4)
        if name == "evaluate":
            command.add_argument("--checkpoint", type=Path, required=True)
        else:
            command.add_argument("--steps", type=int, default=1000)
            command.add_argument("--batch-size", type=int, default=1)
            command.add_argument("--lr", type=float, default=0.001)
            command.add_argument("--seed", type=int, default=42)
            command.add_argument("--log-every", type=int, default=50)
            command.add_argument("--dim", type=int, default=144)
            command.add_argument("--heads", type=int, default=4)
            command.add_argument("--encoder-layers", type=int, default=2)
            command.add_argument("--decoder-layers", type=int, default=2)
            command.add_argument("--patch-size", type=int, default=8)
            command.add_argument("--skip-from", type=int)
            command.add_argument("--skip-to", type=int)
    args = parser.parse_args()
    if args.command == "data":
        if args.url and not args.sha256:
            parser.error("--url requires --sha256")
        if args.archive or args.url:
            archive = args.archive or download(args.url, args.sha256, "dataset.zip")
            print(extract_pt(archive))
        else:
            print(course_dataset(args.scene or "rabbit"))
        return
    if args.size < 1 or args.texture_size < 1 or args.threads < 1:
        parser.error("Resolution, texture size and thread count must be positive")
    torch.set_num_threads(args.threads)
    device = pick_device(args.device)
    dataset = SceneDataset(args.data or course_dataset(args.scene), args.size, args.texture_size)
    if args.command == "evaluate":
        evaluate(args.checkpoint, dataset, device, args.output)
        return
    config = ModelConfig(
        dim=args.dim,
        heads=args.heads,
        encoder_layers=args.encoder_layers,
        decoder_layers=args.decoder_layers,
        patch_size=args.patch_size,
        texture_size=args.texture_size,
        skip_from=args.skip_from,
        skip_to=args.skip_to,
    )
    if args.size % args.patch_size:
        parser.error("Resolution must be divisible by patch size")
    train(
        dataset,
        config,
        args.output,
        steps=args.steps,
        batch_size=args.batch_size,
        learning_rate=args.lr,
        seed=args.seed,
        device=device,
        log_every=args.log_every,
    )
