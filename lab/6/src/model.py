from dataclasses import dataclass
from math import pi, sqrt

import torch
from torch import Tensor, nn
from torch.nn import functional as F

Rotary = tuple[Tensor, Tensor]


@dataclass(frozen=True)
class ModelConfig:
    dim: int = 144
    heads: int = 4
    encoder_layers: int = 2
    decoder_layers: int = 2
    patch_size: int = 8
    texture_size: int = 1
    registers: int = 4
    rope_frequencies: int = 2
    direction_frequencies: int = 4
    skip_from: int | None = None
    skip_to: int | None = None

    def __post_init__(self) -> None:
        for name in (
            "dim",
            "heads",
            "encoder_layers",
            "decoder_layers",
            "patch_size",
            "texture_size",
            "registers",
            "rope_frequencies",
            "direction_frequencies",
        ):
            if getattr(self, name) < 1:
                raise ValueError(f"{name} must be positive")
        if self.dim % self.heads:
            raise ValueError("dim must be divisible by heads")
        if self.dim // self.heads < 18 * self.rope_frequencies:
            raise ValueError("each attention head must fit 9 rotary axes and their pairs")
        if (self.skip_from is None) != (self.skip_to is None):
            raise ValueError("skip_from and skip_to must both be set or both be None")
        if self.skip_from is not None and self.skip_to is not None:
            if not 1 <= self.skip_from < self.skip_to <= self.encoder_layers:
                raise ValueError("encoder skip requires 1 <= skip_from < skip_to <= layers")


def rotary_factors(positions: Tensor, frequencies: Tensor) -> Rotary:
    angles = (positions.unsqueeze(-1) * frequencies).flatten(-2).unsqueeze(1)
    return angles.cos(), angles.sin()


def apply_rotary(x: Tensor, factors: Rotary) -> Tensor:
    cos, sin = (factor.to(dtype=x.dtype) for factor in factors)
    width = 2 * cos.shape[-1]
    pairs = x[..., :width].unflatten(-1, (-1, 2))
    first, second = pairs.unbind(-1)
    rotated = torch.stack((first * cos - second * sin, first * sin + second * cos), -1)
    rotated = rotated.flatten(-2)
    if width == x.shape[-1]:
        return rotated
    return torch.cat((rotated, x[..., width:]), dim=-1)


def scaled_dot_product_attention(query: Tensor, key: Tensor, value: Tensor, valid: Tensor | None = None) -> Tensor:
    mask = None
    if valid is not None:
        mask = valid[:, None, None, :]
        key = key.masked_fill(~mask.transpose(-2, -1), 0)
        value = value.masked_fill(~mask.transpose(-2, -1), 0)
    scores = (query @ key.transpose(-2, -1)) / sqrt(query.shape[-1])
    if mask is not None:
        scores = scores.masked_fill(~mask, -torch.inf)
        scores = torch.where(mask.any(dim=-1, keepdim=True), scores, 0)
    weights = scores.softmax(dim=-1)
    if mask is not None:
        weights = weights.masked_fill(~mask, 0)
    return weights @ value


class MultiHeadAttention(nn.Module):
    def __init__(self, dim: int, heads: int) -> None:
        super().__init__()
        self.heads = heads
        self.head_dim = dim // heads
        self.query = nn.Linear(dim, dim)
        self.key = nn.Linear(dim, dim)
        self.value = nn.Linear(dim, dim)
        self.output = nn.Linear(dim, dim, bias=False)

    def forward(
        self,
        query: Tensor,
        context: Tensor,
        query_rope: Rotary,
        key_rope: Rotary,
        valid: Tensor | None = None,
    ) -> Tensor:
        batch, queries, dim = query.shape
        q = self.query(query).reshape(batch, queries, self.heads, self.head_dim)
        k = self.key(context).reshape(batch, -1, self.heads, self.head_dim)
        v = self.value(context).reshape(batch, -1, self.heads, self.head_dim)
        q = apply_rotary(q.transpose(1, 2), query_rope)
        k = apply_rotary(k.transpose(1, 2), key_rope)
        attended = scaled_dot_product_attention(q, k, v.transpose(1, 2), valid)
        return self.output(attended.transpose(1, 2).reshape(batch, queries, dim))


class EncoderLayer(nn.Module):
    def __init__(self, config: ModelConfig) -> None:
        super().__init__()
        self.attention = MultiHeadAttention(config.dim, config.heads)
        self.attention_norm = nn.LayerNorm(config.dim)
        self.ffn_norm = nn.LayerNorm(config.dim)
        self.ffn = nn.Sequential(
            nn.Linear(config.dim, 4 * config.dim),
            nn.GELU(),
            nn.Linear(4 * config.dim, config.dim),
        )

    def forward(self, x: Tensor, rope: Rotary, valid: Tensor) -> Tensor:
        normalized = self.attention_norm(x)
        x = x + self.attention(normalized, normalized, rope, rope, valid)
        return x + self.ffn(self.ffn_norm(x))


class DecoderLayer(nn.Module):
    def __init__(self, config: ModelConfig) -> None:
        super().__init__()
        self.cross_attention = MultiHeadAttention(config.dim, config.heads)
        self.self_attention = MultiHeadAttention(config.dim, config.heads)
        self.query_norm = nn.LayerNorm(config.dim)
        self.context_norm = nn.LayerNorm(config.dim)
        self.self_norm = nn.LayerNorm(config.dim)
        self.ffn_norm = nn.LayerNorm(config.dim)
        self.ffn = nn.Sequential(
            nn.Linear(config.dim, 4 * config.dim),
            nn.GELU(),
            nn.Linear(4 * config.dim, config.dim),
        )

    def forward(
        self,
        x: Tensor,
        context: Tensor,
        ray_rope: Rotary,
        triangle_rope: Rotary,
        valid: Tensor,
    ) -> Tensor:
        x = x + self.cross_attention(self.query_norm(x), self.context_norm(context), ray_rope, triangle_rope, valid)
        normalized = self.self_norm(x)
        x = x + self.self_attention(normalized, normalized, ray_rope, ray_rope)
        return x + self.ffn(self.ffn_norm(x))


class RenderFormer(nn.Module):
    spatial_frequencies: Tensor
    direction_frequencies: Tensor

    def __init__(self, config: ModelConfig | None = None) -> None:
        super().__init__()
        self.config = config if config is not None else ModelConfig()
        config = self.config
        self.triangle_token = nn.Parameter(torch.empty(1, 1, config.dim))
        self.register_tokens = nn.Parameter(torch.empty(1, config.registers, config.dim))
        self.ray_token = nn.Parameter(torch.empty(1, 1, config.dim))
        for token in (self.triangle_token, self.register_tokens, self.ray_token):
            nn.init.normal_(token, std=0.02)
        self.normal_embedding = nn.Linear(9, config.dim)
        self.material_embedding = nn.Linear(10 * config.texture_size**2, config.dim)
        self.triangle_norm = nn.LayerNorm(config.dim)
        direction_dim = 3 * (1 + 2 * config.direction_frequencies)
        self.ray_embedding = nn.Linear(config.patch_size**2 * direction_dim, config.dim)
        self.ray_norm = nn.LayerNorm(config.dim)
        self.encoder = nn.ModuleList(EncoderLayer(config) for _ in range(config.encoder_layers))
        self.decoder = nn.ModuleList(DecoderLayer(config) for _ in range(config.decoder_layers))
        self.output_norm = nn.LayerNorm(config.dim)
        self.image_head = nn.Linear(config.dim, 3 * config.patch_size**2)
        self.register_buffer("spatial_frequencies", 2.0 ** torch.arange(config.rope_frequencies))
        self.register_buffer("direction_frequencies", pi * 2.0 ** torch.arange(config.direction_frequencies))

    def forward(self, batch: dict[str, Tensor]) -> Tensor:
        valid = batch["valid"]
        positions = batch["positions"].masked_fill(~valid.unsqueeze(-1), 0)
        normals = batch["normals"].masked_fill(~valid.unsqueeze(-1), 0)
        materials = batch["materials"].masked_fill(~valid[:, :, None, None, None], 0)
        rays = batch["rays"]
        batch_size, height, width, _ = rays.shape
        patch = self.config.patch_size
        if height < patch or width < patch or height % patch or width % patch:
            raise ValueError("image height and width must be positive multiples of patch_size")
        if materials.shape[2:] != (10, self.config.texture_size, self.config.texture_size):
            raise ValueError("materials must have 10 channels and the configured texture_size")
        patch_h, patch_w = height // patch, width // patch
        triangles = self.triangle_norm(self.normal_embedding(normals) + self.material_embedding(materials.flatten(2))) + self.triangle_token
        context = torch.cat((self.register_tokens.expand(batch_size, -1, -1), triangles), 1)
        context_valid = torch.cat(
            (
                torch.ones(batch_size, self.config.registers, dtype=torch.bool, device=valid.device),
                valid,
            ),
            dim=1,
        )
        center = positions.sum(1) / valid.sum(1, keepdim=True).clamp_min(1)
        center = center.reshape(batch_size, 3, 3).mean(1).repeat(1, 3)
        context_positions = torch.cat((center[:, None, :].expand(-1, self.config.registers, -1), positions), 1)
        triangle_rope = rotary_factors(context_positions, self.spatial_frequencies)
        skip: Tensor | None = None
        for index, layer in enumerate(self.encoder, start=1):
            if index == self.config.skip_from:
                skip = context
            context = layer(context, triangle_rope, context_valid)
            if index == self.config.skip_to and skip is not None:
                context = context + skip
        angles = rays.unsqueeze(-1) * self.direction_frequencies
        directions = torch.cat((rays, angles.sin().flatten(-2), angles.cos().flatten(-2)), -1)
        patches = directions.reshape(batch_size, patch_h, patch, patch_w, patch, -1)
        patches = patches.permute(0, 1, 3, 2, 4, 5).reshape(batch_size, patch_h * patch_w, -1)
        queries = self.ray_norm(self.ray_embedding(patches)) + self.ray_token
        ray_positions = batch["camera_o"].repeat(1, 3)[:, None, :].expand(-1, patch_h * patch_w, -1)
        ray_rope = rotary_factors(ray_positions, self.spatial_frequencies)
        for layer in self.decoder:
            queries = layer(queries, context, ray_rope, triangle_rope, context_valid)
        pixels = F.softplus(self.image_head(self.output_norm(queries)))
        pixels = pixels.reshape(batch_size, patch_h, patch_w, 3, patch, patch)
        return pixels.permute(0, 3, 1, 4, 2, 5).reshape(batch_size, 3, height, width)
