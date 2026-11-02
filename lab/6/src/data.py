from __future__ import annotations

import hashlib
import json
import os
import shutil
import sys
import tempfile
import urllib.request
import zipfile
from pathlib import Path
from typing import Any
from urllib.parse import urlencode

import torch
import torch.nn.functional as F
from torch import Tensor

COURSE_DATASETS = {
    "rabbit": (
        "pt_dataset_rabbit_v2.zip",
        "0ab73b73825f4f8999a52756438ae74a03f995696dd22323b2683fd7b0c4af2d",
    ),
    "teapot": (
        "pt_dataset_teapot_v2.zip",
        "63c967e1d2225f4a329a3a3d4e30fbc8618f5ee354b6cc8a6f4a147c2ea0cec0",
    ),
}


def course_dataset(name: str) -> Path:
    filename, digest = COURSE_DATASETS[name]
    cached = cache_root() / "downloads" / digest / filename
    if not cached.exists() or sha256(cached) != digest:
        query = urlencode({"share_id": "e0763966076744bfa29a", "path": "/" + filename})
        resolver = "https://pan.ustc.edu.cn/api/v1/share/get_share_file_download?" + query
        with urllib.request.urlopen(resolver, timeout=60) as response:
            metadata = json.load(response)
        if metadata.get("code") != 1:
            raise RuntimeError(f"Course dataset resolver failed: {metadata}")
        cached = download(metadata["data"]["download_link"], digest, filename)
    return extract_pt(cached)


def cache_root() -> Path:
    if root := os.environ.get("SDU_CG_CACHE"):
        return Path(root).expanduser()
    base = Path(os.environ.get("XDG_CACHE_HOME", Path.home() / ".cache"))
    if sys.platform == "darwin" and "XDG_CACHE_HOME" not in os.environ:
        base = Path.home() / "Library" / "Caches"
    return base / "sdu-cg-lab6"


def sha256(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def download(url: str, digest: str, name: str) -> Path:
    if not url.startswith("https://"):
        raise ValueError("Dataset downloads require an HTTPS URL")
    if len(digest) != 64 or any(c not in "0123456789abcdef" for c in digest):
        raise ValueError("Provide the expected lowercase SHA-256 digest")
    if Path(name).name != name or name in {"", ".", ".."}:
        raise ValueError("Dataset name must be a single filename")
    root = cache_root() / "downloads" / digest
    root.mkdir(parents=True, exist_ok=True)
    target = root / name
    if target.exists() and sha256(target) == digest:
        return target
    with tempfile.NamedTemporaryFile(dir=root, delete=False) as stream:
        temporary = Path(stream.name)
        try:
            request = urllib.request.Request(url, headers={"User-Agent": "SDU-CG-Lab6/1.0"})
            with urllib.request.urlopen(request, timeout=120) as response:
                shutil.copyfileobj(response, stream)
            stream.close()
            if sha256(temporary) != digest:
                raise ValueError(f"SHA-256 mismatch for {url}")
            temporary.replace(target)
        finally:
            temporary.unlink(missing_ok=True)
    return target


def extract_pt(archive: Path) -> Path:
    digest = sha256(archive)
    target = cache_root() / "datasets" / digest
    if (target / ".complete").exists():
        return target
    target.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=target.parent) as directory:
        staging = Path(directory)
        count = 0
        with zipfile.ZipFile(archive) as bundle:
            for info in bundle.infolist():
                if not info.filename.endswith(".pt"):
                    continue
                destination = (staging / info.filename).resolve()
                if not destination.is_relative_to(staging.resolve()):
                    raise ValueError(f"Unsafe archive member: {info.filename}")
                if (info.external_attr >> 16) & 0o170000 == 0o120000:
                    raise ValueError(f"Archive links are unsupported: {info.filename}")
                destination.parent.mkdir(parents=True, exist_ok=True)
                with bundle.open(info) as source, destination.open("wb") as output:
                    shutil.copyfileobj(source, output)
                count += 1
        if not count:
            raise ValueError(f"No PT samples in {archive}")
        (staging / ".complete").write_text(digest, encoding="utf-8")
        if target.exists():
            raise FileExistsError(f"Incomplete dataset cache: {target}; remove it and retry")
        staging.rename(target)
    return target


def _tensor(value: Any) -> Tensor:
    return torch.as_tensor(value, dtype=torch.float32)


def _objects(value: Any) -> Tensor:
    return torch.cat([_tensor(item) for item in value], dim=0)


def prepare_sample(sample: dict[str, Any], size: int, texture_size: int) -> dict[str, Tensor]:
    positions = _objects(sample["scene_objects_pos"]).reshape(-1, 9)
    normals = _objects(sample["scene_objects_normals"]).reshape(-1, 9)
    materials = _objects(sample["scene_objects_patches"])
    if materials.shape[1] == 13:
        materials = torch.cat((materials[:, :7], materials[:, 10:13]), dim=1)
    elif materials.shape[1] != 10:
        raise ValueError(f"Expected 10 or 13 material channels, got {materials.shape[1]}")
    materials = F.adaptive_avg_pool2d(materials, (texture_size, texture_size))
    materials[:, 7:10] = materials[:, 7:10].clamp_min(0).log1p()
    target = _tensor(sample["gt_image"])
    if target.ndim != 3 or target.shape[0] != 3:
        raise ValueError("gt_image must have shape [3,H,W]")
    rays = _tensor(sample["ray_map"])
    if rays.shape != (*target.shape[1:], 3):
        raise ValueError("Ray map dimensions do not match the image")
    if size > min(target.shape[-2:]):
        raise ValueError("Requested resolution exceeds the supplied ground truth")
    target = F.interpolate(target[None], (size, size), mode="area")[0]
    rays = F.interpolate(rays.permute(2, 0, 1)[None], (size, size), mode="bilinear", align_corners=False)[0].permute(1, 2, 0)
    result = {
        "positions": positions,
        "normals": normals,
        "materials": materials,
        "valid": torch.ones(positions.shape[0], dtype=torch.bool),
        "camera_o": _tensor(sample["camera_o"]).reshape(3),
        "rays": F.normalize(rays, dim=-1),
        "target": target.clamp_min(0),
    }
    if normals.shape != positions.shape or materials.shape[0] != positions.shape[0]:
        raise ValueError("Triangle, normal and material counts differ")
    if not positions.shape[0] or not all(torch.isfinite(t).all() for t in result.values()):
        raise ValueError("Scene is empty or contains non-finite values")
    return result


class SceneDataset:
    def __init__(self, root: Path, size: int = 64, texture_size: int = 1):
        self.root = root.expanduser().resolve()
        self.size = size
        self.texture_size = texture_size
        self.samples: list[dict[str, Tensor]] = []
        self.names: list[str] = []
        self.fingerprint = ""
        files = sorted(self.root.rglob("*.pt")) if self.root.is_dir() else [self.root]
        if not files:
            raise ValueError(f"No PT samples under {self.root}")
        digest = hashlib.sha256()
        for path in files:
            digest.update(bytes.fromhex(sha256(path)))
            sample = torch.load(path, map_location="cpu", weights_only=True)
            self.samples.append(prepare_sample(sample, size, texture_size))
            self.names.append(path.relative_to(self.root).as_posix())
        self.fingerprint = digest.hexdigest()
        explicit_val = [i for i, name in enumerate(self.names) if "val" in Path(name).parts]
        explicit_test = [i for i, name in enumerate(self.names) if "test" in Path(name).parts]
        if explicit_val:
            self.val_indices = explicit_val
            self.train_indices = [i for i in range(len(files)) if i not in explicit_val and i not in explicit_test]
        else:
            usable = [i for i in range(len(files)) if i not in explicit_test]
            self.val_indices = usable[4::5]
            self.train_indices = [i for i in usable if i not in self.val_indices]
        if not self.train_indices or not self.val_indices:
            raise ValueError("Need training and validation views (at least five PT samples)")

    def batch(self, indices: list[int], device: torch.device) -> dict[str, Tensor]:
        samples = [self.samples[index] for index in indices]
        triangles = max(sample["positions"].shape[0] for sample in samples)
        batch: dict[str, Tensor] = {}
        for key in samples[0]:
            values = []
            for sample in samples:
                value = sample[key]
                if key in {"positions", "normals", "materials", "valid"}:
                    padding = triangles - value.shape[0]
                    if padding:
                        value = torch.cat((value, value.new_zeros((padding, *value.shape[1:]))))
                values.append(value)
            batch[key] = torch.stack(values).to(device)
        return batch

    def metadata(self) -> dict[str, Any]:
        return {
            "fingerprint": self.fingerprint,
            "resolution": self.size,
            "texture_size": self.texture_size,
            "train_views": [self.names[i] for i in self.train_indices],
            "validation_views": [self.names[i] for i in self.val_indices],
            "triangles_min": min(s["positions"].shape[0] for s in self.samples),
            "triangles_max": max(s["positions"].shape[0] for s in self.samples),
        }
