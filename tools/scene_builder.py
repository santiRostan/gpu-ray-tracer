#!/usr/bin/env python3
"""Build XML scenes from higher-level TOML scene specs."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

from scene_spec import SceneSpecError, lint_scene, load_scene_spec, make_draft_scene, write_scene_xml


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("spec", help="Path to the TOML scene spec")
    parser.add_argument(
        "--strict",
        action="store_true",
        help="Treat linter warnings as build failures",
    )
    parser.add_argument(
        "--no-lint",
        action="store_true",
        help="Skip the linter pass before writing XML",
    )
    parser.add_argument(
        "--draft",
        action="store_true",
        help="Write a draft scene variant using [draft_render] overrides or built-in defaults",
    )
    return parser


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

    messages = [] if args.no_lint else lint_scene(scene)
    errors = [message for message in messages if message.severity == "error"]
    warnings = [message for message in messages if message.severity == "warning"]

    for message in messages:
        prefix = message.severity.upper()
        if message.item_ids:
            item_list = ", ".join(message.item_ids)
            print(f"{prefix}: {message.text} [{item_list}]")
        else:
            print(f"{prefix}: {message.text}")

    if errors or (args.strict and warnings):
        print("Scene XML was not written because lint checks failed.", file=sys.stderr)
        return 1

    output_path = write_scene_xml(scene)
    print(f"Wrote {Path(output_path).resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
