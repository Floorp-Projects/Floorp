#!/usr/bin/env python3
"""Extract the Lepton-specific chrome-extras toggles from vendored leptonChrome.css.

Run from browser-features/skin/lepton/css:
    python3 ../../../../tools/gen-chrome-extras.py

Writes one CSS file per toggle into
browser-features/chrome/common/designs/chrome-extras-css/

A block is emitted only when its `@media` condition gates on exactly one Lepton
toggle: the toggle itself must appear, and no other Lepton sub-option may appear
as a positive term. `not (...)`-wrapped terms only narrow a block, so they are
ignored. Blocks that AND several Lepton toggles together are layout
combinations and are left to the native implementations; the omissions are
recorded in each generated header so the divergence stays auditable.

Only `-moz-bool-pref:` gated blocks are considered. Vendored Lepton also uses a
bare `-moz-pref(...)` form in a large region of the file; we do not port from
there (see the module comment in designs/chrome-extras.ts).
"""

from __future__ import annotations

import os
import re
import sys

SRC = "leptonChrome.css"
OUT = "../../../chrome/common/designs/chrome-extras-css"

LINES = open(SRC, encoding="utf-8").read().split("\n")

BOOLPREF = re.compile(r'-moz-bool-pref:\s*"([^"]+)"')

# Lepton sub-options its own user.js enables by default. `icon.menu` blocks that
# also require one of these are part of the effective behaviour, so the extra
# condition is dropped rather than the block being discarded.
# `icon.menu` blocks are additionally guarded by `not (icon.disabled)`; that is the
# icon-disabled toggle itself, so it is allowed as a companion condition.
DEFAULT_ON_SUBOPTS = [
    "userChrome.icon.disabled",
    "userChrome.icon.context_menu",
    "userChrome.icon.global_menu",
    "userChrome.icon.global_menubar",
    "userChrome.icon.library",
]
MENU_OMITTED = [
    "userChrome.icon.menu.full",
    "userChrome.theme.non_native_menu",
    "userChrome.padding.global_menubar",
]


def top_blocks() -> list[tuple[int, int, str, list[str], int]]:
    """Every `@media` block at depth 0 or directly inside `@-moz-document`.

    Returns (startLine, endLine, condition, bodyLines, indentLevel) where
    indentLevel is 0 for a top-level block and 1 for one nested inside an
    `@-moz-document` wrapper.
    """
    n = len(LINES)
    out: list[tuple[int, int, str, list[str], int]] = []
    i = 0
    while i < n:
        line = LINES[i]
        if re.match(r"^\s*@media\b", line):
            start = i
            # A media query can span several lines (comma-separated groups), so
            # the opening brace is not necessarily on the `@media` line. Walk
            # forward until the braces balance back to zero.
            depth = 0
            seen_open = False
            j = i
            while j < n:
                opens = LINES[j].count("{")
                depth += opens - LINES[j].count("}")
                if opens:
                    seen_open = True
                if seen_open and depth == 0:
                    break
                j += 1
            # condition text: everything up to the brace that opens depth 1,
            # including continuation lines of a comma-separated media query.
            # `cond_end` is the line index that carries that brace.
            cond_lines: list[str] = []
            d = 0
            k = start
            cond_end = start
            while k < n:
                s = LINES[k]
                idx = s.find("{")
                if idx >= 0 and d == 0:
                    cond_lines.append(s[:idx])
                    cond_end = k
                    break
                d += s.count("{") - s.count("}")
                cond_lines.append(s)
                k += 1
            cond = " ".join(x.strip() for x in cond_lines)
            # indentation of the body tells us whether we are nested
            body_start = cond_end + 1
            indent = 1 if LINES[body_start].startswith("    ") else 0
            out.append((start + 1, j + 1, cond, LINES[body_start:j], indent))
            i = j + 1
        else:
            i += 1
    return out


def split_terms(cond: str) -> tuple[list[str], list[str]]:
    """Return (positive, negative) pref names mentioned by the condition."""
    pos: list[str] = []
    neg: list[str] = []
    for m in BOOLPREF.finditer(cond):
        before = cond[max(0, m.start() - 12):m.start()]
        # `not (` immediately precedes the term (allow whitespace)
        if re.search(r"not\s*\(\s*$", before):
            neg.append(m.group(1))
        else:
            pos.append(m.group(1))
    return pos, neg


def unwrap(body: list[str], indent: int) -> str:
    """Drop the block's own indentation level, trim blank edges."""
    pad = "  " * (indent + 1)
    out: list[str] = []
    for ln in body:
        if ln.startswith(pad):
            out.append(ln[len(pad):])
        elif ln.strip() == "":
            out.append("")
        else:
            out.append(ln)
    while out and out[0].strip() == "":
        out.pop(0)
    while out and out[-1].strip() == "":
        out.pop()
    return "\n".join(out)


def is_lepton_pref(name: str) -> bool:
    return name.startswith("userChrome.")


def emit(
    fname: str,
    key: str,
    provenance: str,
    body: str,
    notes: list[str] | None = None,
) -> None:
    header = [
        f"/* Floorp chrome-extras: {key}",
        f" * Ported from vendored Lepton ({provenance}).",
        " *",
        " * The `@media (-moz-bool-pref: ...)` gate has been removed — whether this",
        " * sheet is injected is decided in JS (ui-custom/styles/style-manager.ts).",
        " * That also drops Floorp's dependency on the vendored file, which",
        " * update_lepton.yml re-syncs from upstream every day.",
    ]
    for note in notes or []:
        header.append(" *")
        for line in note.split("\n"):
            header.append(f" * {line}".rstrip())
    header.append(" */")
    text = "\n".join(header) + "\n\n" + body.rstrip() + "\n"
    with open(os.path.join(OUT, fname), "w", encoding="utf-8") as fh:
        fh.write(text)
    print(f"  {fname}: {len(body.splitlines())} lines")


def select(pref: str, polarity: str = "positive", allow: list[str] | None = None):
    """Blocks whose condition gates on exactly `pref`.

    `polarity` is "positive" for `-moz-bool-pref: "pref"` and "negative" for
    `not (-moz-bool-pref: "pref")`. `allow` lists extra Lepton sub-options that
    may also appear as positive terms without disqualifying the block.
    """
    allow = allow or []
    hit: list[tuple[int, int, list[str], list[str], int]] = []
    for s, e, cond, body, indent in top_blocks():
        pos, neg = split_terms(cond)
        own = pos if polarity == "positive" else neg
        if pref not in own:
            continue
        # What else must be true for the block to match is always the set of
        # *positive* terms. `not (...)` terms only narrow a block, so they never
        # disqualify it — that is how `hidden.tab_icon` without `.always` works.
        requirements = [p for p in pos if p != pref]
        blockers = [
            p for p in requirements if is_lepton_pref(p) and p not in allow
        ]
        if blockers:
            continue
        dropped = [p for p in requirements if is_lepton_pref(p)]
        hit.append((s, e, dropped, body, indent))
    return hit


def write(
    fname: str,
    key: str,
    hit,
    notes: list[str] | None = None,
    extra: str = "",
) -> None:
    parts = []
    for s, e, dropped, body, indent in hit:
        note = f"/* leptonChrome.css:{s}-{e}"
        if dropped:
            note += f" — extra conditions dropped: {', '.join(dropped)}"
        parts.append(note + " */\n" + unwrap(body, indent))
    prov = (
        f"leptonChrome.css {hit[0][0]}-{hit[-1][1]}, {len(hit)} block(s)"
        if hit else "n/a"
    )
    body = "\n\n".join(parts) if parts else "/* no rules */"
    if extra:
        body += "\n\n" + extra.rstrip() + "\n"
    emit(fname, key, prov, body, notes)


def main() -> int:
    if not os.path.exists(SRC):
        print("run this from browser-features/skin/lepton/css", file=sys.stderr)
        return 1
    os.makedirs(OUT, exist_ok=True)
    print("generating chrome-extras stylesheets:")

    write("autohide-tab.css", "autohideTab", select("userChrome.autohide.tab"), [
        "Only the `margin-bottom` behaviour is ported. The `autohide.tab.opacity`",
        "and `autohide.tab.blur` variants are separate Lepton prefs that Floorp",
        "does not expose.",
    ])
    # Lepton emits `#sidebar-splitter { display: none }` in the same block that
    # defines the `--uc-sidebar-*` tokens, gated on sidebar.overlap OR
    # autohide.sidebar. The tokens move to the always-present scaffold (they are
    # inert when unused) but the rule must stay conditional, so it is appended
    # to both sheets that want it.
    SIDEBAR_SPLITTER = """/* leptonChrome.css:25174-25176 (shared with sidebar-overlap.css) */
#sidebar-splitter {
  display: none !important;
}"""

    write("autohide-sidebar.css", "autohideSidebar",
          select("userChrome.autohide.sidebar"), [
        "Needs the `--uc-sidebar-*` tokens from scaffold.css. Lepton's dedicated",
        "`sidebar.overlap and autohide.sidebar` combination block is not ported;",
        "with both toggles on, the two sheets merge instead.",
    ], extra=SIDEBAR_SPLITTER)
    write("autohide-back-button.css", "autohideBackButton",
          select("userChrome.autohide.back_button"), [
        "Lepton wraps these in",
        '`@-moz-document url("chrome://browser/content/browser.xhtml")`; the',
        "wrapper is dropped because Floorp injects into that document directly.",
    ])
    write("autohide-forward-button.css", "autohideForwardButton",
          select("userChrome.autohide.forward_button"))
    write("autohide-page-action.css", "autohidePageAction",
          select("userChrome.autohide.page_action"), [
        "The extra transition block gated on `userChrome.decoration.animate` is",
        "not ported (Lepton-only sub-option).",
    ])
    write("hidden-tab-icon.css", "hiddenTabIcon", select("userChrome.hidden.tab_icon"), [
        "The `hidden.tab_icon.always` variant (hides .tab-icon-stack instead of",
        ".tab-icon-image) is not ported (Lepton-only sub-option).",
    ])
    write("hidden-tabbar.css", "hiddenTabbar", select("userChrome.hidden.tabbar"))
    write("hidden-navbar.css", "hiddenNavbar", select("userChrome.hidden.navbar"))
    write("hidden-sidebar-header.css", "hiddenSidebarHeader",
          select("userChrome.hidden.sidebar_header"), [
        "The `hidden.sidebar_header.vertical_tab_only` variant is not ported",
        "(Lepton-only sub-option).",
    ])
    write("hidden-urlbar-iconbox.css", "hiddenUrlbarIconbox",
          select("userChrome.hidden.urlbar_iconbox"), [
        "The `hidden.urlbar_iconbox.label_only` variant is not ported",
        "(Lepton-only sub-option).",
    ])
    write("hidden-bookmarkbar-icon.css", "hiddenBookmarkbarIcon",
          select("userChrome.hidden.bookmarkbar_icon"))
    write("hidden-bookmarkbar-label.css", "hiddenBookmarkbarLabel",
          select("userChrome.hidden.bookmarkbar_label"))
    write("hidden-disabled-menu.css", "hiddenDisabledMenu",
          select("userChrome.hidden.disabled_menu"), [
        "The `widget.*.native-context-menus` branches are kept verbatim — those",
        "are genuine Gecko prefs, not Lepton toggles.",
    ])
    write("centered-tab.css", "centeredTab", select("userChrome.centered.tab"), [
        "The `centered.tab.label`, `tab.close_button_at_hover` and `counter.tab`",
        "variants are Lepton-only sub-options and are not ported.",
    ])
    write("centered-urlbar.css", "centeredUrlbar", select("userChrome.centered.urlbar"))
    write("centered-bookmarkbar.css", "centeredBookmarkbar",
          select("userChrome.centered.bookmarkbar"))
    write("url-view-move-icon-to-left.css", "urlViewMoveIconToLeft",
          select("userChrome.urlView.move_icon_to_left"))
    write("url-view-go-button-when-typing.css", "urlViewGoButtonWhenTyping",
          select("userChrome.urlView.go_button_when_typing"))
    write("url-view-always-show-page-actions.css",
          "urlViewAlwaysShowPageActions",
          select("userChrome.urlbar.always_show_page_actions"), [
        "Lepton gates this on `userChrome.urlbar.always_show_page_actions` while",
        "Floorp's settings page wrote `userChrome.urlView.always_show_page_actions`,",
        "so the toggle never did anything. Porting from the Lepton name fixes it.",
    ])
    write("sidebar-overlap.css", "sidebarOverlap", select("userChrome.sidebar.overlap"), [
        "Needs the `--uc-sidebar-*` tokens from scaffold.css. Lepton's dedicated",
        "`sidebar.overlap and autohide.sidebar` combination block is not ported;",
        "with both toggles on, the two sheets merge instead.",
    ], extra=SIDEBAR_SPLITTER)

    write("icon-disabled.css", "iconDisabled",
          select("userChrome.icon.disabled", polarity="negative",
                 allow=["userChrome.icon.panel"]), [
        "This sheet is applied while icons are ENABLED, i.e. when the toggle is",
        'OFF — Lepton\'s gate was `@media not (-moz-bool-pref:',
        '"userChrome.icon.disabled")`. The `icon.library` and',
        "`tab.connect_to_window` variants are Lepton-only sub-options and are not",
        "ported.",
        "",
        "`icon.panel` blocks ARE kept: they carry the Floorp-only panel menu IDs",
        "(`#rebootappmenu`, `#appMenu-ssb-button`, ...) that used to live in",
        "`FLOORP_ICON_PATCHES` and would otherwise be lost.",
    ])

    hit = select("userChrome.icon.menu", polarity="positive",
                 allow=DEFAULT_ON_SUBOPTS)
    omitted = 0
    for s, e, cond, body, _indent in top_blocks():
        pos, _neg = split_terms(cond)
        if "userChrome.icon.menu" not in pos:
            continue
        if any(p in MENU_OMITTED for p in pos):
            omitted += 1
    write("icon-menu.css", "iconMenu", hit, [
        "Applied only while icons are enabled (iconDisabled off).",
        f"{omitted} block(s) gated on Lepton-only sub-options "
        f"({', '.join(MENU_OMITTED)}) are not ported — they stay Lepton-family-only.",
    ])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
