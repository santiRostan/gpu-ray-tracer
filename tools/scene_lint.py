#!/usr/bin/env python3
"""Lint a TOML scene spec without writing XML."""

from __future__ import annotations

import argparse
import sys

from scene_spec import SceneSpecError, lint_scene, load_scene_spec


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("spec", help="Path to the TOML scene spec")
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    try:
        scene = load_scene_spec(args.spec)
    except SceneSpecError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    messages = lint_scene(scene)
    if not messages:
        print("No lint findings.")
        return 0

    exit_code = 0
    for message in messages:
        prefix = message.severity.upper()
        if message.item_ids:
            item_list = ", ".join(message.item_ids)
            print(f"{prefix}: {message.text} [{item_list}]")
        else:
            print(f"{prefix}: {message.text}")
        if message.severity == "error":
            exit_code = 1

    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
