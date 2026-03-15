#!/usr/bin/env python3
"""Lint, build, and optionally render a scene spec."""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys

from scene_spec import SceneSpecError, lint_scene, load_scene_spec, make_draft_scene, write_scene_xml


ROOT_DIR = Path(__file__).resolve().parents[1]


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("spec", help="Path to the TOML scene spec")
    parser.add_argument("--draft", action="store_true", help="Render the draft variant")
    parser.add_argument("--strict", action="store_true", help="Treat lint warnings as failures")
    parser.add_argument("--build-only", action="store_true", help="Write XML but skip launching the renderer")
    parser.add_argument("--rebuild", action="store_true", help="Run `cmake --build` before rendering")
    parser.add_argument("--config", default="Release", help="Build configuration to use for the executable")
    parser.add_argument(
        "--exe",
        help="Optional path to gpu_raytracer.exe; defaults to build/bin/<config>/gpu_raytracer.exe",
    )
    return parser


def resolve_executable(args: argparse.Namespace) -> Path:
    if args.exe:
        exe_path = Path(args.exe)
        if not exe_path.is_absolute():
            exe_path = ROOT_DIR / exe_path
        return exe_path
    return ROOT_DIR / "build" / "bin" / args.config / "gpu_raytracer.exe"


def print_messages(messages: list) -> tuple[bool, bool]:
    has_errors = False
    has_warnings = False
    for message in messages:
        prefix = message.severity.upper()
        if message.item_ids:
            item_list = ", ".join(message.item_ids)
            print(f"{prefix}: {message.text} [{item_list}]")
        else:
            print(f"{prefix}: {message.text}")
        if message.severity == "error":
            has_errors = True
        elif message.severity == "warning":
            has_warnings = True
    return has_errors, has_warnings


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    try:
        scene = load_scene_spec(args.spec)
    except SceneSpecError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    if args.draft:
        scene = make_draft_scene(scene)

    messages = lint_scene(scene)
    has_errors, has_warnings = print_messages(messages)
    if has_errors or (args.strict and has_warnings):
        print("Scene render aborted because lint checks failed.", file=sys.stderr)
        return 1

    output_path = write_scene_xml(scene)
    print(f"Wrote {output_path.resolve()}", flush=True)

    if args.build_only:
        return 0

    exe_path = resolve_executable(args)
    if args.rebuild or not exe_path.exists():
        subprocess.run(
            ["cmake", "--build", "build", "--config", args.config],
            cwd=ROOT_DIR,
            check=True,
        )

    if not exe_path.exists():
        print(f"error: executable not found at {exe_path}", file=sys.stderr)
        return 1

    subprocess.run(
        [str(exe_path), "--scene", str(output_path.resolve())],
        cwd=exe_path.parent,
        check=True,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
