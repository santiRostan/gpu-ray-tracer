#!/usr/bin/env python3
"""Shared scene-spec parsing, resolution, linting, and XML export helpers."""

from __future__ import annotations

from dataclasses import dataclass, field, replace
from pathlib import Path
import math
import tomllib
import xml.etree.ElementTree as ET


ROOT_DIR = Path(__file__).resolve().parents[1]


class SceneSpecError(RuntimeError):
    """Raised when a scene spec cannot be resolved."""


@dataclass
class RenderSettings:
    image_width: int = 1280
    image_height: int = 720
    samples_per_pixel: int = 1500
    max_depth: int = 20


@dataclass
class EnvironmentSettings:
    texture: str | None = None
    intensity: float = 1.0
    rotation_degrees: float = 0.0


@dataclass
class ToneMappingSettings:
    exposure: float = 1.0
    gamma: float = 2.2


@dataclass
class BloomSettings:
    threshold: float = 1.0
    intensity: float = 0.3


@dataclass
class FilmSettings:
    tone_mapping: ToneMappingSettings | None = None
    bloom: BloomSettings | None = None


@dataclass
class CameraSettings:
    position: tuple[float, float, float]
    look_at: tuple[float, float, float]
    up: tuple[float, float, float] = (0.0, 1.0, 0.0)
    fov: float = 45.0
    aperture: float = 0.0
    focus_distance: float = 1.0


@dataclass
class MaterialSpec:
    material_id: str
    material_type: str
    color: tuple[float, float, float] | None = None
    fuzz: float | None = None
    refraction_index: float | None = None


@dataclass
class ResolvedItem:
    item_id: str
    kind: str
    material: str
    placement: str
    center: tuple[float, float, float]
    source_index: int
    radius: float | None = None
    size: tuple[float, float, float] | None = None
    size_2d: tuple[float, float] | None = None
    corner: tuple[float, float, float] | None = None
    u_edge: tuple[float, float, float] | None = None
    v_edge: tuple[float, float, float] | None = None
    rotation_y: float = 0.0
    target: str | None = None
    offset: tuple[float, float, float] = (0.0, 0.0, 0.0)


@dataclass
class ResolvedScene:
    name: str
    output_path: Path
    render: RenderSettings
    environment: EnvironmentSettings
    film: FilmSettings
    camera: CameraSettings
    materials: list[MaterialSpec]
    items: list[ResolvedItem]
    draft_render: RenderSettings | None = None
    material_ids: set[str] = field(default_factory=set)


@dataclass
class LintMessage:
    severity: str
    text: str
    item_ids: tuple[str, ...] = ()


def _as_path(base_dir: Path, value: str | None, default_name: str) -> Path:
    if value:
        path = Path(value)
        if not path.is_absolute():
            path = ROOT_DIR / path
        return path
    return ROOT_DIR / "scenes" / f"{default_name}.xml"


def _parse_vec(value: object, size: int, label: str) -> tuple[float, ...]:
    if not isinstance(value, list) or len(value) != size:
        raise SceneSpecError(f"{label} must be a list of {size} numbers")
    try:
        return tuple(float(component) for component in value)
    except (TypeError, ValueError) as exc:
        raise SceneSpecError(f"{label} must contain only numbers") from exc


def _parse_optional_vec(value: object | None, size: int, default: tuple[float, ...], label: str) -> tuple[float, ...]:
    if value is None:
        return default
    return _parse_vec(value, size, label)


def _vec_add(left: tuple[float, float, float], right: tuple[float, float, float]) -> tuple[float, float, float]:
    return (left[0] + right[0], left[1] + right[1], left[2] + right[2])


def _vec_sub(left: tuple[float, float, float], right: tuple[float, float, float]) -> tuple[float, float, float]:
    return (left[0] - right[0], left[1] - right[1], left[2] - right[2])


def _vec_cross(left: tuple[float, float, float], right: tuple[float, float, float]) -> tuple[float, float, float]:
    return (
        left[1] * right[2] - left[2] * right[1],
        left[2] * right[0] - left[0] * right[2],
        left[0] * right[1] - left[1] * right[0],
    )


def _vec_length(vector: tuple[float, float, float]) -> float:
    return math.sqrt(vector[0] * vector[0] + vector[1] * vector[1] + vector[2] * vector[2])


def _quad_components(item: ResolvedItem) -> tuple[tuple[float, float, float], tuple[float, float, float], tuple[float, float, float]]:
    if item.kind == "floor_quad":
        half_x = item.size_2d[0] * 0.5
        half_z = item.size_2d[1] * 0.5
        corner = (item.center[0] - half_x, item.center[1], item.center[2] - half_z)
        return (corner, (item.size_2d[0], 0.0, 0.0), (0.0, 0.0, item.size_2d[1]))
    if item.kind == "quad":
        return (item.corner, item.u_edge, item.v_edge)
    raise SceneSpecError(f"Unsupported quad request for item kind: {item.kind}")


def _quad_vertices(item: ResolvedItem) -> list[tuple[float, float, float]]:
    corner, u_edge, v_edge = _quad_components(item)
    return [
        corner,
        _vec_add(corner, u_edge),
        _vec_add(corner, v_edge),
        _vec_add(_vec_add(corner, u_edge), v_edge),
    ]


def _quad_normal(item: ResolvedItem) -> tuple[float, float, float]:
    _, u_edge, v_edge = _quad_components(item)
    normal = _vec_cross(u_edge, v_edge)
    length = _vec_length(normal)
    if length <= 1e-6:
        raise SceneSpecError(f"Quad {item.item_id!r} is degenerate")
    return (normal[0] / length, normal[1] / length, normal[2] / length)


def _top_y(item: ResolvedItem) -> float:
    if item.kind == "sphere":
        return item.center[1] + float(item.radius)
    if item.kind == "box":
        return item.center[1] + float(item.size[1]) * 0.5
    if item.kind in {"floor_quad", "quad"}:
        return max(vertex[1] for vertex in _quad_vertices(item))
    raise SceneSpecError(f"Unsupported item kind: {item.kind}")


def _bottom_y(item: ResolvedItem) -> float:
    if item.kind == "sphere":
        return item.center[1] - float(item.radius)
    if item.kind == "box":
        return item.center[1] - float(item.size[1]) * 0.5
    if item.kind in {"floor_quad", "quad"}:
        return min(vertex[1] for vertex in _quad_vertices(item))
    raise SceneSpecError(f"Unsupported item kind: {item.kind}")


def _base_height(raw_item: dict[str, object], kind: str, index: int) -> float:
    if kind == "sphere":
        radius = float(raw_item["radius"])
        if radius <= 0.0:
            raise SceneSpecError(f"items[{index}] sphere radius must be positive")
        return radius
    if kind == "box":
        size = _parse_vec(raw_item.get("size"), 3, f"items[{index}].size")
        if min(size) <= 0.0:
            raise SceneSpecError(f"items[{index}] box size components must be positive")
        return size[1] * 0.5
    if kind in {"floor_quad", "quad"}:
        return 0.0
    raise SceneSpecError(f"items[{index}] has unsupported kind {kind!r}")


def _resolve_item(
    raw_item: dict[str, object],
    index: int,
    materials: set[str],
    resolved_by_id: dict[str, ResolvedItem],
) -> ResolvedItem:
    kind = str(raw_item.get("kind", "")).strip()
    if kind not in {"floor_quad", "quad", "sphere", "box"}:
        raise SceneSpecError(f"items[{index}] has unsupported kind {kind!r}")

    item_id = str(raw_item.get("id") or f"item_{index}")
    material = str(raw_item.get("material", "")).strip()
    if material not in materials:
        raise SceneSpecError(f"items[{index}] references unknown material {material!r}")

    placement = str(raw_item.get("placement", "absolute"))
    center: tuple[float, float, float]
    target = None
    offset = (0.0, 0.0, 0.0)

    if kind == "floor_quad":
        center = _parse_optional_vec(raw_item.get("center"), 3, (0.0, 0.0, 0.0), f"items[{index}].center")
    elif kind == "quad":
        placement = "absolute"
        center = (0.0, 0.0, 0.0)
    elif placement == "absolute":
        center = _parse_vec(raw_item.get("center"), 3, f"items[{index}].center")
    elif placement == "on_floor":
        at = _parse_vec(raw_item.get("at"), 2, f"items[{index}].at")
        base_height = _base_height(raw_item, kind, index)
        center = (at[0], base_height, at[1])
    elif placement == "on_top_of":
        target = str(raw_item.get("target", "")).strip()
        if not target:
            raise SceneSpecError(f"items[{index}] placement 'on_top_of' requires a target")
        parent = resolved_by_id.get(target)
        if parent is None:
            raise SceneSpecError(
                f"items[{index}] target {target!r} must appear earlier in the spec or exist already"
            )
        if parent.kind not in {"sphere", "box"}:
            raise SceneSpecError(
                f"items[{index}] placement 'on_top_of' currently supports only sphere/box targets, got {parent.kind!r}"
            )
        offset = _parse_optional_vec(raw_item.get("offset"), 3, (0.0, 0.0, 0.0), f"items[{index}].offset")
        base_height = _base_height(raw_item, kind, index)
        center = (
            parent.center[0] + offset[0],
            _top_y(parent) + base_height + offset[1],
            parent.center[2] + offset[2],
        )
    else:
        raise SceneSpecError(f"items[{index}] has unsupported placement {placement!r}")

    if kind == "sphere":
        radius = float(raw_item["radius"])
        return ResolvedItem(
            item_id=item_id,
            kind=kind,
            material=material,
            placement=placement,
            center=center,
            source_index=index,
            radius=radius,
            target=target,
            offset=offset,
        )

    if kind == "box":
        size = _parse_vec(raw_item.get("size"), 3, f"items[{index}].size")
        rotation_y = float(raw_item.get("rotation_y", 0.0))
        return ResolvedItem(
            item_id=item_id,
            kind=kind,
            material=material,
            placement=placement,
            center=center,
            source_index=index,
            size=size,
            rotation_y=rotation_y,
            target=target,
            offset=offset,
        )

    if kind == "quad":
        corner = _parse_vec(raw_item.get("corner"), 3, f"items[{index}].corner")
        u_edge = _parse_vec(raw_item.get("u_edge"), 3, f"items[{index}].u_edge")
        v_edge = _parse_vec(raw_item.get("v_edge"), 3, f"items[{index}].v_edge")
        if _vec_length(_vec_cross(u_edge, v_edge)) <= 1e-6:
            raise SceneSpecError(f"items[{index}] quad edges produce zero area")
        return ResolvedItem(
            item_id=item_id,
            kind=kind,
            material=material,
            placement=placement,
            center=center,
            source_index=index,
            corner=corner,
            u_edge=u_edge,
            v_edge=v_edge,
        )

    size_2d = _parse_vec(raw_item.get("size"), 2, f"items[{index}].size")
    return ResolvedItem(
        item_id=item_id,
        kind=kind,
        material=material,
        placement=placement,
        center=center,
        source_index=index,
        size_2d=size_2d,
    )


def _bounds_for_items(items: list[ResolvedItem]) -> tuple[tuple[float, float, float], tuple[float, float, float]]:
    mins = [float("inf"), float("inf"), float("inf")]
    maxs = [float("-inf"), float("-inf"), float("-inf")]

    for item in items:
        item_min, item_max = _item_bounds(item)
        for axis in range(3):
            mins[axis] = min(mins[axis], item_min[axis])
            maxs[axis] = max(maxs[axis], item_max[axis])

    return (tuple(mins), tuple(maxs))


def _normalize_vec3(vector: tuple[float, float, float], label: str) -> tuple[float, float, float]:
    length = math.sqrt(vector[0] * vector[0] + vector[1] * vector[1] + vector[2] * vector[2])
    if length <= 1e-6:
        raise SceneSpecError(f"{label} must not be the zero vector")
    return (vector[0] / length, vector[1] / length, vector[2] / length)


def _resolve_camera(
    camera_raw: dict[str, object],
    items: list[ResolvedItem],
    render: RenderSettings,
) -> CameraSettings:
    up = _parse_optional_vec(camera_raw.get("up"), 3, (0.0, 1.0, 0.0), "camera.up")
    fov = float(camera_raw.get("fov", 45.0))
    aperture = float(camera_raw.get("aperture", 0.0))
    explicit_focus = float(camera_raw["focus_distance"]) if "focus_distance" in camera_raw else None

    if "position" in camera_raw and "look_at" in camera_raw:
        position = _parse_vec(camera_raw.get("position"), 3, "camera.position")
        look_at = _parse_vec(camera_raw.get("look_at"), 3, "camera.look_at")
        focus_distance = explicit_focus if explicit_focus is not None else math.dist(position, look_at)
        return CameraSettings(
            position=position,
            look_at=look_at,
            up=up,
            fov=fov,
            aperture=aperture,
            focus_distance=focus_distance,
        )

    solids = [item for item in items if item.kind not in {"floor_quad", "quad"}]
    if not solids:
        raise SceneSpecError("camera framing requires at least one non-floor object")

    if "frame_target" in camera_raw:
        target_id = str(camera_raw.get("frame_target", "")).strip()
        target_items = [item for item in solids if item.item_id == target_id]
        if not target_items:
            raise SceneSpecError(f"camera.frame_target references unknown item {target_id!r}")
    else:
        frame_mode = str(camera_raw.get("frame", "all")).strip()
        if frame_mode != "all":
            raise SceneSpecError("camera.frame currently supports only 'all'; use frame_target for single items")
        target_items = solids

    bounds_min, bounds_max = _bounds_for_items(target_items)
    look_at = tuple(
        (bounds_min[axis] + bounds_max[axis]) * 0.5 for axis in range(3)
    )
    target_offset = _parse_optional_vec(camera_raw.get("target_offset"), 3, (0.0, 0.0, 0.0), "camera.target_offset")
    look_at = tuple(look_at[axis] + target_offset[axis] for axis in range(3))

    subject_radius = max(
        math.dist(bounds_min, bounds_max) * 0.5,
        0.25,
    )
    view_direction = _normalize_vec3(
        _parse_optional_vec(camera_raw.get("view_direction"), 3, (0.08, 0.18, 1.0), "camera.view_direction"),
        "camera.view_direction",
    )
    aspect_ratio = render.image_width / render.image_height
    vertical_fov = math.radians(fov)
    horizontal_fov = 2.0 * math.atan(math.tan(vertical_fov * 0.5) * aspect_ratio)
    effective_fov = min(vertical_fov, horizontal_fov)
    distance_scale = float(camera_raw.get("distance_scale", 1.25))
    framing_distance = subject_radius / math.tan(effective_fov * 0.5)
    distance = framing_distance * distance_scale
    position = tuple(look_at[axis] + view_direction[axis] * distance for axis in range(3))
    focus_distance = explicit_focus if explicit_focus is not None else math.dist(position, look_at)
    return CameraSettings(
        position=position,
        look_at=look_at,
        up=up,
        fov=fov,
        aperture=aperture,
        focus_distance=focus_distance,
    )


def load_scene_spec(spec_path: str | Path) -> ResolvedScene:
    spec_path = Path(spec_path)
    with spec_path.open("rb") as spec_file:
        raw_spec = tomllib.load(spec_file)

    name = str(raw_spec.get("name", "")).strip()
    if not name:
        raise SceneSpecError("Spec must define a top-level 'name'")

    render_raw = raw_spec.get("render", {})
    render = RenderSettings(
        image_width=int(render_raw.get("image_width", 1280)),
        image_height=int(render_raw.get("image_height", 720)),
        samples_per_pixel=int(render_raw.get("samples_per_pixel", 1500)),
        max_depth=int(render_raw.get("max_depth", 20)),
    )

    environment_raw = raw_spec.get("environment", {})
    environment = EnvironmentSettings(
        texture=str(environment_raw["texture"]).strip() if "texture" in environment_raw else None,
        intensity=float(environment_raw.get("intensity", 1.0)),
        rotation_degrees=float(environment_raw.get("rotation_degrees", 0.0)),
    )

    film_raw = raw_spec.get("film", {})
    tone_mapping = None
    bloom = None
    if "tone_mapping" in film_raw:
        tone_raw = film_raw["tone_mapping"]
        tone_mapping = ToneMappingSettings(
            exposure=float(tone_raw.get("exposure", 1.0)),
            gamma=float(tone_raw.get("gamma", 2.2)),
        )
    if "bloom" in film_raw:
        bloom_raw = film_raw["bloom"]
        bloom = BloomSettings(
            threshold=float(bloom_raw.get("threshold", 1.0)),
            intensity=float(bloom_raw.get("intensity", 0.3)),
        )
    film = FilmSettings(tone_mapping=tone_mapping, bloom=bloom)

    camera_raw = raw_spec.get("camera")
    if not camera_raw:
        raise SceneSpecError("Spec must define a [camera] section")

    materials_raw = raw_spec.get("materials", [])
    if not isinstance(materials_raw, list) or not materials_raw:
        raise SceneSpecError("Spec must define at least one [[materials]] entry")

    materials: list[MaterialSpec] = []
    material_ids: set[str] = set()
    for index, material_raw in enumerate(materials_raw):
        material_id = str(material_raw.get("id", "")).strip()
        material_type = str(material_raw.get("type", "")).strip()
        if not material_id or not material_type:
            raise SceneSpecError(f"materials[{index}] must define id and type")
        if material_id in material_ids:
            raise SceneSpecError(f"Duplicate material id {material_id!r}")
        material_ids.add(material_id)

        color = None
        if "color" in material_raw:
            color = _parse_vec(material_raw["color"], 3, f"materials[{index}].color")

        materials.append(
            MaterialSpec(
                material_id=material_id,
                material_type=material_type,
                color=color,
                fuzz=float(material_raw["fuzz"]) if "fuzz" in material_raw else None,
                refraction_index=(
                    float(material_raw["refraction_index"]) if "refraction_index" in material_raw else None
                ),
            )
        )

    items_raw = raw_spec.get("items", [])
    if not isinstance(items_raw, list) or not items_raw:
        raise SceneSpecError("Spec must define at least one [[items]] entry")

    items: list[ResolvedItem] = []
    resolved_by_id: dict[str, ResolvedItem] = {}
    for index, raw_item in enumerate(items_raw):
        item = _resolve_item(raw_item, index, material_ids, resolved_by_id)
        if item.item_id in resolved_by_id:
            raise SceneSpecError(f"Duplicate item id {item.item_id!r}")
        items.append(item)
        resolved_by_id[item.item_id] = item

    camera = _resolve_camera(camera_raw, items, render)

    draft_render_raw = raw_spec.get("draft_render")
    draft_render = None
    if isinstance(draft_render_raw, dict):
        draft_render = RenderSettings(
            image_width=int(draft_render_raw.get("image_width", 640)),
            image_height=int(draft_render_raw.get("image_height", 360)),
            samples_per_pixel=int(draft_render_raw.get("samples_per_pixel", 128)),
            max_depth=int(draft_render_raw.get("max_depth", min(render.max_depth, 12))),
        )

    output_path = _as_path(spec_path.parent, raw_spec.get("output"), name)
    return ResolvedScene(
        name=name,
        output_path=output_path,
        render=render,
        environment=environment,
        film=film,
        camera=camera,
        materials=materials,
        items=items,
        draft_render=draft_render,
        material_ids=material_ids,
    )


def _box_extents(item: ResolvedItem) -> tuple[float, float, float]:
    half_x = item.size[0] * 0.5
    half_y = item.size[1] * 0.5
    half_z = item.size[2] * 0.5
    radians = math.radians(item.rotation_y)
    cos_theta = abs(math.cos(radians))
    sin_theta = abs(math.sin(radians))
    extent_x = cos_theta * half_x + sin_theta * half_z
    extent_z = sin_theta * half_x + cos_theta * half_z
    return (extent_x, half_y, extent_z)


def _item_bounds(item: ResolvedItem) -> tuple[tuple[float, float, float], tuple[float, float, float]]:
    if item.kind == "sphere":
        radius = float(item.radius)
        return (
            (item.center[0] - radius, item.center[1] - radius, item.center[2] - radius),
            (item.center[0] + radius, item.center[1] + radius, item.center[2] + radius),
        )
    if item.kind == "box":
        extents = _box_extents(item)
        return (
            (item.center[0] - extents[0], item.center[1] - extents[1], item.center[2] - extents[2]),
            (item.center[0] + extents[0], item.center[1] + extents[1], item.center[2] + extents[2]),
        )
    if item.kind in {"floor_quad", "quad"}:
        vertices = _quad_vertices(item)
        return (
            tuple(min(vertex[axis] for vertex in vertices) for axis in range(3)),
            tuple(max(vertex[axis] for vertex in vertices) for axis in range(3)),
        )
    raise SceneSpecError(f"Unsupported item kind: {item.kind}")


def _overlap_1d(a_min: float, a_max: float, b_min: float, b_max: float) -> float:
    return min(a_max, b_max) - max(a_min, b_min)


def _sphere_box_penetration(sphere: ResolvedItem, box: ResolvedItem) -> float:
    box_min, box_max = _item_bounds(box)
    closest_x = min(max(sphere.center[0], box_min[0]), box_max[0])
    closest_y = min(max(sphere.center[1], box_min[1]), box_max[1])
    closest_z = min(max(sphere.center[2], box_min[2]), box_max[2])
    dx = sphere.center[0] - closest_x
    dy = sphere.center[1] - closest_y
    dz = sphere.center[2] - closest_z
    distance = math.sqrt(dx * dx + dy * dy + dz * dz)
    return float(sphere.radius) - distance


def _camera_look_at_in_bounds(scene: ResolvedScene) -> bool:
    solids = [item for item in scene.items if item.kind not in {"floor_quad", "quad"}]
    if not solids:
        return True

    mins, maxs = _bounds_for_items(solids)

    margin = 1.5
    look_at = scene.camera.look_at
    return all(mins[axis] - margin <= look_at[axis] <= maxs[axis] + margin for axis in range(3))


def _is_floor_support_surface(item: ResolvedItem, tolerance: float) -> bool:
    if item.kind not in {"floor_quad", "quad"}:
        return False

    vertices = _quad_vertices(item)
    if any(abs(vertex[1]) > tolerance for vertex in vertices):
        return False

    try:
        normal = _quad_normal(item)
    except SceneSpecError:
        return False

    return abs(normal[1]) > 0.9


def lint_scene(scene: ResolvedScene, tolerance: float = 0.02) -> list[LintMessage]:
    messages: list[LintMessage] = []
    support_surfaces = [item for item in scene.items if _is_floor_support_surface(item, tolerance)]

    for item in scene.items:
        if item.kind in {"floor_quad", "quad"}:
            try:
                area_measure = _vec_length(_vec_cross(*_quad_components(item)[1:]))
            except SceneSpecError as exc:
                messages.append(
                    LintMessage(
                        "error",
                        str(exc),
                        (item.item_id,),
                    )
                )
                continue

            if area_measure <= tolerance:
                messages.append(
                    LintMessage(
                        "error",
                        f"{item.item_id} is degenerate and has near-zero area",
                        (item.item_id,),
                    )
                )
            continue

        if _bottom_y(item) < -tolerance:
            messages.append(
                LintMessage(
                    "error",
                    f"{item.item_id} extends below y=0 and will clip through the floor plane",
                    (item.item_id,),
                )
            )

        if item.placement == "absolute" and _bottom_y(item) > tolerance:
            messages.append(
                LintMessage(
                    "warning",
                    f"{item.item_id} uses absolute placement and may look like it is floating",
                    (item.item_id,),
                )
            )

        if item.placement == "on_floor" and not support_surfaces:
            messages.append(
                LintMessage(
                    "warning",
                    f"{item.item_id} is placed on_floor but the scene has no upward quad at y=0 to visually support it",
                    (item.item_id,),
                )
            )

    solids = [item for item in scene.items if item.kind in {"sphere", "box"}]
    for left_index, left in enumerate(solids):
        for right in solids[left_index + 1 :]:
            if left.kind == "sphere" and right.kind == "sphere":
                distance = math.dist(left.center, right.center)
                penetration = float(left.radius) + float(right.radius) - distance
                if penetration > tolerance:
                    messages.append(
                        LintMessage(
                            "error",
                            f"{left.item_id} overlaps {right.item_id} by about {penetration:.3f} units",
                            (left.item_id, right.item_id),
                        )
                    )
                continue

            if left.kind == "box" and right.kind == "box":
                left_min, left_max = _item_bounds(left)
                right_min, right_max = _item_bounds(right)
                overlap_x = _overlap_1d(left_min[0], left_max[0], right_min[0], right_max[0])
                overlap_y = _overlap_1d(left_min[1], left_max[1], right_min[1], right_max[1])
                overlap_z = _overlap_1d(left_min[2], left_max[2], right_min[2], right_max[2])
                if overlap_x > tolerance and overlap_y > tolerance and overlap_z > tolerance:
                    messages.append(
                        LintMessage(
                            "error",
                            f"{left.item_id} overlaps {right.item_id} in 3D space",
                            (left.item_id, right.item_id),
                        )
                    )
                continue

            sphere = left if left.kind == "sphere" else right
            box = right if left.kind == "sphere" else left
            penetration = _sphere_box_penetration(sphere, box)
            if penetration > tolerance:
                messages.append(
                    LintMessage(
                        "error",
                        f"{sphere.item_id} intersects {box.item_id} by about {penetration:.3f} units",
                        (sphere.item_id, box.item_id),
                    )
                )

    if not _camera_look_at_in_bounds(scene):
        messages.append(
            LintMessage(
                "warning",
                "camera.look_at sits well outside the main object bounds, so framing may feel off",
            )
        )

    return messages


def _write_vec_attributes(element: ET.Element, labels: tuple[str, ...], values: tuple[float, ...]) -> None:
    for label, value in zip(labels, values):
        element.set(label, f"{value:.6g}")


def scene_to_xml(scene: ResolvedScene) -> ET.ElementTree:
    root = ET.Element("scene", {"name": scene.name})

    render = ET.SubElement(root, "render")
    ET.SubElement(render, "image_width").text = str(scene.render.image_width)
    ET.SubElement(render, "image_height").text = str(scene.render.image_height)
    ET.SubElement(render, "samples_per_pixel").text = str(scene.render.samples_per_pixel)
    ET.SubElement(render, "max_depth").text = str(scene.render.max_depth)

    if scene.environment.texture:
        environment = ET.SubElement(root, "environment")
        ET.SubElement(environment, "texture").text = scene.environment.texture
        ET.SubElement(environment, "intensity").text = f"{scene.environment.intensity:.6g}"
        ET.SubElement(environment, "rotation_degrees").text = f"{scene.environment.rotation_degrees:.6g}"

    if scene.film.tone_mapping or scene.film.bloom:
        film = ET.SubElement(root, "film")
        if scene.film.tone_mapping:
            tone = ET.SubElement(film, "tone_mapping")
            ET.SubElement(tone, "exposure").text = f"{scene.film.tone_mapping.exposure:.6g}"
            ET.SubElement(tone, "gamma").text = f"{scene.film.tone_mapping.gamma:.6g}"
        if scene.film.bloom:
            bloom = ET.SubElement(film, "bloom")
            ET.SubElement(bloom, "threshold").text = f"{scene.film.bloom.threshold:.6g}"
            ET.SubElement(bloom, "intensity").text = f"{scene.film.bloom.intensity:.6g}"

    camera = ET.SubElement(root, "camera")
    position = ET.SubElement(camera, "position")
    _write_vec_attributes(position, ("x", "y", "z"), scene.camera.position)
    look_at = ET.SubElement(camera, "look_at")
    _write_vec_attributes(look_at, ("x", "y", "z"), scene.camera.look_at)
    up = ET.SubElement(camera, "up")
    _write_vec_attributes(up, ("x", "y", "z"), scene.camera.up)
    ET.SubElement(camera, "fov").text = f"{scene.camera.fov:.6g}"
    ET.SubElement(camera, "aperture").text = f"{scene.camera.aperture:.6g}"
    ET.SubElement(camera, "focus_distance").text = f"{scene.camera.focus_distance:.6g}"

    materials = ET.SubElement(root, "materials")
    for material_spec in scene.materials:
        material = ET.SubElement(
            materials,
            "material",
            {"id": material_spec.material_id, "type": material_spec.material_type},
        )
        if material_spec.color is not None:
            color = ET.SubElement(material, "color")
            _write_vec_attributes(color, ("r", "g", "b"), material_spec.color)
        if material_spec.fuzz is not None:
            ET.SubElement(material, "fuzz").text = f"{material_spec.fuzz:.6g}"
        if material_spec.refraction_index is not None:
            ET.SubElement(material, "refraction_index").text = f"{material_spec.refraction_index:.6g}"

    objects = ET.SubElement(root, "objects")
    for item in scene.items:
        if item.kind == "sphere":
            sphere = ET.SubElement(objects, "sphere", {"id": item.item_id, "material": item.material})
            center = ET.SubElement(sphere, "center")
            _write_vec_attributes(center, ("x", "y", "z"), item.center)
            ET.SubElement(sphere, "radius").text = f"{item.radius:.6g}"
            continue

        if item.kind == "box":
            box = ET.SubElement(objects, "box", {"id": item.item_id, "material": item.material})
            center = ET.SubElement(box, "center")
            _write_vec_attributes(center, ("x", "y", "z"), item.center)
            size = ET.SubElement(box, "size")
            _write_vec_attributes(size, ("x", "y", "z"), item.size)
            if abs(item.rotation_y) > 1e-6:
                rotation = ET.SubElement(box, "rotation")
                rotation.set("y", f"{item.rotation_y:.6g}")
            continue

        quad = ET.SubElement(objects, "quad", {"id": item.item_id, "material": item.material})
        corner_value, u_edge_value, v_edge_value = _quad_components(item)
        corner = ET.SubElement(quad, "corner")
        _write_vec_attributes(corner, ("x", "y", "z"), corner_value)
        u_edge = ET.SubElement(quad, "u_edge")
        _write_vec_attributes(u_edge, ("x", "y", "z"), u_edge_value)
        v_edge = ET.SubElement(quad, "v_edge")
        _write_vec_attributes(v_edge, ("x", "y", "z"), v_edge_value)

    tree = ET.ElementTree(root)
    ET.indent(tree, space="  ")
    return tree


def write_scene_xml(scene: ResolvedScene) -> Path:
    scene.output_path.parent.mkdir(parents=True, exist_ok=True)
    tree = scene_to_xml(scene)
    tree.write(scene.output_path, encoding="utf-8", xml_declaration=True)
    return scene.output_path


def make_draft_scene(scene: ResolvedScene) -> ResolvedScene:
    draft_render = scene.draft_render or RenderSettings(
        image_width=640,
        image_height=360,
        samples_per_pixel=128,
        max_depth=min(scene.render.max_depth, 12),
    )
    draft_output = scene.output_path.with_name(f"{scene.output_path.stem}_draft.xml")
    return replace(
        scene,
        name=f"{scene.name}_draft",
        output_path=draft_output,
        render=draft_render,
    )
