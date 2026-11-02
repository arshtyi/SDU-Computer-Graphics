"""Consumer-visible data contracts: HDR, material layout, masks and held-out views."""

import tempfile
import unittest
import zipfile
from math import log
from pathlib import Path
from unittest.mock import patch

import torch

from data import SceneDataset, extract_pt, prepare_sample
from train import psnr, tone_map


def sample(triangles: int = 2) -> dict:
    material = torch.zeros(triangles, 13, 2, 2)
    material[:, :7] = 0.25
    material[:, 7:10] = 99
    material[:, 10:13] = 3
    return {
        "scene_objects_pos": [torch.ones(triangles, 9)],
        "scene_objects_normals": [torch.ones(triangles, 9)],
        "scene_objects_patches": [material],
        "gt_image": torch.full((3, 4, 4), 50.0),
        "camera_o": torch.zeros(3),
        "ray_map": torch.tensor([0.0, 0.0, -1.0]).expand(4, 4, 3),
    }


class DataTests(unittest.TestCase):
    def test_hdr_and_thirteen_channel_materials_survive_preprocessing(self) -> None:
        result = prepare_sample(sample(), 2, 1)
        torch.testing.assert_close(result["target"], torch.full((3, 2, 2), 50.0))
        torch.testing.assert_close(result["materials"][:, :7], torch.full((2, 7, 1, 1), 0.25))
        torch.testing.assert_close(result["materials"][:, 7:], torch.full((2, 3, 1, 1), log(4.0)))
        torch.testing.assert_close(result["rays"].norm(dim=-1), torch.ones(2, 2))

    def test_missing_ground_truth_is_not_silently_replaced(self) -> None:
        value = sample()
        del value["gt_image"]
        with self.assertRaises(KeyError):
            prepare_sample(value, 2, 1)

    def test_nonfinite_geometry_is_rejected(self) -> None:
        value = sample()
        value["scene_objects_pos"][0][0, 0] = torch.nan
        with self.assertRaisesRegex(ValueError, "non-finite"):
            prepare_sample(value, 2, 1)

    def test_explicit_validation_and_test_views_do_not_leak_into_training(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for split in ("train", "val", "test"):
                (root / split).mkdir()
                torch.save(sample(), root / split / "00000.pt")
            dataset = SceneDataset(root, 2)
            self.assertEqual([dataset.names[i] for i in dataset.train_indices], ["train/00000.pt"])
            self.assertEqual([dataset.names[i] for i in dataset.val_indices], ["val/00000.pt"])

    def test_implicit_split_is_disjoint_and_batch_padding_is_masked(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for index in range(10):
                torch.save(sample(1 + index % 2), root / f"{index:05d}.pt")
            dataset = SceneDataset(root, 2)
            self.assertEqual(dataset.val_indices, [4, 9])
            self.assertEqual(set(dataset.train_indices) & set(dataset.val_indices), set())
            self.assertEqual(set(dataset.train_indices) | set(dataset.val_indices), set(range(10)))
            batch = dataset.batch([0, 1], torch.device("cpu"))
            torch.testing.assert_close(batch["valid"], torch.tensor([[True, False], [True, True]]))
            torch.testing.assert_close(batch["positions"][0, 1], torch.zeros(9))

    def test_archive_cannot_escape_cache(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            archive = root / "bad.zip"
            with zipfile.ZipFile(archive, "w") as bundle:
                bundle.writestr("../../escaped.pt", b"untrusted")
            with patch.dict("os.environ", {"SDU_CG_CACHE": str(root / "cache")}):
                with self.assertRaisesRegex(ValueError, "Unsafe archive"):
                    extract_pt(archive)
            self.assertFalse((root / "escaped.pt").exists())

    def test_metric_uses_fixed_reinhard_not_per_image_normalization(self) -> None:
        torch.testing.assert_close(tone_map(torch.tensor([0.0, 1.0, 3.0, 99.0])), torch.tensor([0.0, 0.5, 0.75, 0.99]))
        self.assertAlmostEqual(psnr(0.01), 20.0)


if __name__ == "__main__":
    unittest.main()
