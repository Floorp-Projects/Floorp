#!/usr/bin/env python3
"""Rename the `design.lepton-preferences` i18n namespace to `design.chrome-extras`.

The chrome-extras toggles are no longer Lepton-specific, so the settings page and
its translation keys are renamed to match. Each locale file keeps the block in
the position it already occupies, so the diff stays a rename rather than a move.

Run from the repo root:
    python3 tools/rename-lepton-i18n.py
"""

from __future__ import annotations

import collections
import glob
import json
import os
import sys

LOCALES = "browser-features/pages-settings/src/lib/i18n/locales/*.json"

OLD = "lepton-preferences"
NEW = "chrome-extras"

# English and Japanese are re-worded: the strings mentioned Lepton by name.
OVERRIDES = {
    "en-US": {
        "title": "UI Extension Settings",
        "description": (
            "Apply optional UI tweaks on top of the currently selected design"
        ),
        "configure": "Open UI extension settings",
    },
    "ja-JP": {
        "title": "UI 拡張設定",
        "description": "選択中のデザインに、追加の UI 調整を適用します",
        "configure": "UI 拡張設定を開く",
    },
}

# Keys dropped because the UI that used them is gone.
DROP_KEYS = ("leptonRepository", "visitRepository")

# `configureLepton` becomes `configure`: the button label on the design page.
RENAME_KEYS = {"configureLepton": "configure"}


def rewrite_block(block: object, locale: str) -> collections.OrderedDict:
    assert isinstance(block, collections.OrderedDict)
    out = collections.OrderedDict()
    for key, value in block.items():
        if key in RENAME_KEYS:
            out[RENAME_KEYS[key]] = value
        elif key in DROP_KEYS:
            continue
        elif key == "experimentalWarning" and isinstance(value, dict):
            out[key] = collections.OrderedDict(
                (k, v) for k, v in value.items() if k not in DROP_KEYS
            )
        else:
            out[key] = value
    out.update(OVERRIDES.get(locale, {}))
    return out


def rename_namespace(node: object, locale: str) -> None:
    """Rename `OLD` to `NEW` in place, preserving key order."""
    if isinstance(node, collections.OrderedDict):
        items = list(node.items())
        if OLD in node:
            renamed = rewrite_block(node[OLD], locale)
            rebuilt = collections.OrderedDict(
                (NEW, renamed) if key == OLD else (key, value)
                for key, value in items
            )
            node.clear()
            node.update(rebuilt)
            return
        for value in node.values():
            rename_namespace(value, locale)
    elif isinstance(node, list):
        for item in node:
            rename_namespace(item, locale)


def main() -> int:
    files = sorted(glob.glob(LOCALES))
    if not files:
        print("no locale files found", file=sys.stderr)
        return 1
    for path in files:
        locale = os.path.basename(path)[: -len(".json")]
        with open(path, encoding="utf-8") as fh:
            data = json.load(fh, object_pairs_hook=collections.OrderedDict)
        serialized = json.dumps(data, ensure_ascii=False)
        rename_namespace(data, locale)
        if OLD in json.dumps(data, ensure_ascii=False):
            print(f"!! {locale}: still contains {OLD}", file=sys.stderr)
            return 1
        if NEW not in json.dumps(data, ensure_ascii=False):
            print(f"!! {locale}: {NEW} was not created", file=sys.stderr)
            return 1
        # Preserve the repo's 4-space indentation and trailing newline.
        with open(path, "w", encoding="utf-8") as fh:
            json.dump(data, fh, ensure_ascii=False, indent=4)
            fh.write("\n")
        del serialized
    print(f"renamed {OLD} -> {NEW} in {len(files)} locale files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
