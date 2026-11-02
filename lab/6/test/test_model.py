"""Behavioral checks for the explicit attention and world-coordinate renderer."""

import unittest
from dataclasses import replace

import torch
from torch import Tensor
from torch.nn import functional as F

from model import (
    ModelConfig,
    RenderFormer,
    apply_rotary,
    rotary_factors,
    scaled_dot_product_attention,
)


class AttentionTests(unittest.TestCase):
    def setUp(self) -> None:
        torch.manual_seed(42)

    def test_attention_matches_sdpa_outputs_and_gradients(self) -> None:
        query = torch.randn(2, 3, 4, 6, dtype=torch.float64, requires_grad=True)
        key = torch.randn(2, 3, 5, 6, dtype=torch.float64, requires_grad=True)
        value = torch.randn(2, 3, 5, 7, dtype=torch.float64, requires_grad=True)
        valid = torch.tensor([[True, False, True, True, False], [False] * 5])
        for mask in (None, valid):
            with self.subTest(masked=mask is not None):
                actual = scaled_dot_product_attention(query, key, value, mask)
                expected = F.scaled_dot_product_attention(
                    query,
                    key,
                    value,
                    attn_mask=None if mask is None else mask[:, None, None, :],
                )
                torch.testing.assert_close(actual, expected)
                upstream = torch.randn_like(actual)
                actual_grad = torch.autograd.grad(actual, (query, key, value), upstream)
                expected_grad = torch.autograd.grad(expected, (query, key, value), upstream)
                for result, reference in zip(actual_grad, expected_grad, strict=True):
                    torch.testing.assert_close(result, reference)
                if mask is not None:
                    torch.testing.assert_close(actual[1], torch.zeros_like(actual[1]))

    def test_masked_keys_and_values_do_not_change_attention(self) -> None:
        query = torch.randn(2, 2, 3, 8)
        key = torch.randn(2, 2, 4, 8)
        value = torch.randn(2, 2, 4, 8)
        valid = torch.tensor([[True, True, False, False], [False] * 4])
        expected = scaled_dot_product_attention(query, key, value, valid)
        padding = ~valid[:, None, :, None]
        actual = scaled_dot_product_attention(
            query,
            key.masked_fill(padding, torch.nan),
            value.masked_fill(padding, torch.nan),
            valid,
        )
        self.assertTrue(torch.isfinite(actual).all().item())
        torch.testing.assert_close(actual, expected)

    def test_rotary_cross_attention_is_translation_invariant(self) -> None:
        query = torch.randn(2, 2, 3, 38, dtype=torch.float64)
        key = torch.randn(2, 2, 5, 38, dtype=torch.float64)
        query_positions = torch.randn(2, 3, 9, dtype=torch.float64)
        key_positions = torch.randn(2, 5, 9, dtype=torch.float64)
        frequencies = torch.tensor([1.0, 2.0], dtype=torch.float64)
        translation = torch.tensor([2.0, -3.0, 0.4], dtype=torch.float64).repeat(3)

        def scores(offset: Tensor) -> Tensor:
            q = apply_rotary(query, rotary_factors(query_positions + offset, frequencies))
            k = apply_rotary(key, rotary_factors(key_positions + offset, frequencies))
            return q @ k.transpose(-2, -1)

        torch.testing.assert_close(scores(torch.zeros(9)), scores(translation))


class RenderFormerTests(unittest.TestCase):
    def setUp(self) -> None:
        torch.manual_seed(17)
        self.config = ModelConfig(
            dim=36,
            heads=2,
            encoder_layers=2,
            decoder_layers=1,
            patch_size=2,
            registers=2,
            rope_frequencies=1,
            direction_frequencies=2,
        )
        self.model = RenderFormer(self.config).eval()

    def batch(self, height: int = 4, width: int = 6) -> dict[str, Tensor]:
        return {
            "positions": torch.randn(2, 3, 9),
            "normals": F.normalize(torch.randn(2, 3, 3, 3), dim=-1).flatten(2),
            "materials": torch.rand(2, 3, 10, 1, 1),
            "valid": torch.tensor([[True, True, False], [True, False, False]]),
            "camera_o": torch.randn(2, 3),
            "rays": F.normalize(torch.randn(2, height, width, 3), dim=-1),
        }

    def test_batched_square_and_nonsquare_rendering(self) -> None:
        for height, width in ((4, 4), (4, 6), (6, 2)):
            with self.subTest(height=height, width=width), torch.no_grad():
                rendered = self.model(self.batch(height, width))
                self.assertEqual(rendered.shape, (2, 3, height, width))
                self.assertTrue(torch.isfinite(rendered).all().item())
                self.assertTrue((rendered >= 0).all().item())

    def test_hdr_output_can_exceed_one(self) -> None:
        with torch.no_grad():
            self.model.image_head.weight.zero_()
            self.model.image_head.bias.fill_(4)
            rendered = self.model(self.batch())
        self.assertTrue((rendered > 1).all().item())

    def test_padding_values_and_additional_padding_are_irrelevant(self) -> None:
        batch = self.batch()
        padded = {name: tensor.clone() for name, tensor in batch.items()}
        for name in ("positions", "normals", "materials"):
            tensor = padded[name]
            mask = ~batch["valid"].reshape(2, 3, *([1] * (tensor.ndim - 2)))
            tensor = tensor.masked_fill(mask, torch.nan)
            extra = torch.full((2, 2, *tensor.shape[2:]), torch.nan)
            padded[name] = torch.cat((tensor, extra), 1)
        padded["valid"] = torch.cat((batch["valid"], torch.zeros(2, 2, dtype=torch.bool)), 1)
        with torch.no_grad():
            torch.testing.assert_close(self.model(batch), self.model(padded), atol=1e-6, rtol=1e-5)

    def test_empty_geometry_stays_finite(self) -> None:
        batch = self.batch()
        batch["valid"].fill_(False)
        for name in ("positions", "normals", "materials"):
            batch[name].fill_(torch.nan)
        with torch.no_grad():
            self.assertTrue(torch.isfinite(self.model(batch)).all().item())
        for name in ("positions", "normals", "materials", "valid"):
            batch[name] = batch[name][:, :0]
        with torch.no_grad():
            self.assertTrue(torch.isfinite(self.model(batch)).all().item())

    def test_geometry_material_normal_and_view_sensitivity(self) -> None:
        batch = self.batch()
        with torch.no_grad():
            reference = self.model(batch)
            for name in ("positions", "normals", "materials", "camera_o", "rays"):
                with self.subTest(input=name):
                    changed = {key: value.clone() for key, value in batch.items()}
                    if name == "rays":
                        changed[name] = F.normalize(changed[name] + 0.5, dim=-1)
                    else:
                        changed[name][:, 0] += 0.7
                    difference = (self.model(changed) - reference).abs().max().item()
                    self.assertGreater(difference, 1e-6)

    def test_scene_and_camera_translation_preserves_render(self) -> None:
        batch = self.batch()
        translated = {name: tensor.clone() for name, tensor in batch.items()}
        translation = torch.tensor([0.3, -0.8, 1.2])
        translated["positions"] += translation.repeat(3)
        translated["camera_o"] += translation
        with torch.no_grad():
            torch.testing.assert_close(self.model(batch), self.model(translated), atol=1e-6, rtol=1e-5)

    def test_backward_reaches_scene_and_view_inputs(self) -> None:
        batch = self.batch()
        names = ("positions", "normals", "materials", "camera_o", "rays")
        for name in names:
            batch[name].requires_grad_()
        output = self.model(batch)
        loss = (output * torch.randn_like(output)).sum()
        gradients = torch.autograd.grad(loss, tuple(batch[name] for name in names))
        for name, gradient in zip(names, gradients, strict=True):
            with self.subTest(input=name):
                self.assertTrue(torch.isfinite(gradient).all().item())
                self.assertGreater(gradient.abs().max().item(), 0)
                if name in ("positions", "normals", "materials"):
                    padding_gradient = gradient[~batch["valid"]]
                    torch.testing.assert_close(padding_gradient, torch.zeros_like(padding_gradient))

    def test_valid_skip_supports_forward_and_backward(self) -> None:
        model = RenderFormer(replace(self.config, skip_from=1, skip_to=2))
        output = model(self.batch())
        output.mean().backward()
        self.assertTrue(torch.isfinite(output).all().item())
        for parameter in model.parameters():
            self.assertIsNotNone(parameter.grad)
            if parameter.grad is not None:
                self.assertTrue(torch.isfinite(parameter.grad).all().item())

    def test_invalid_skip_and_rotary_dimensions_are_rejected(self) -> None:
        for source, target in ((1, None), (None, 2), (0, 2), (2, 1), (1, 3), (1, 1)):
            with self.subTest(source=source, target=target), self.assertRaises(ValueError):
                replace(self.config, skip_from=source, skip_to=target)
        with self.assertRaises(ValueError):
            replace(self.config, dim=35)
        with self.assertRaises(ValueError):
            replace(self.config, rope_frequencies=2)

    def test_nondivisible_image_dimensions_are_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, "multiples of patch_size"):
            self.model(self.batch(3, 4))


if __name__ == "__main__":
    unittest.main()
