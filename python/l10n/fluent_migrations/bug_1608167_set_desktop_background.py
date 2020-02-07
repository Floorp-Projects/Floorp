# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from

def migrate(ctx):
    """Bug 1608167 - Migrate setDesktopBackground to Fluent, part {index}."""

    ctx.add_transforms(
        "browser/browser/setDesktopBackground.ftl",
        "browser/browser/setDesktopBackground.ftl",
    transforms_from(
"""
set-desktop-background-window =
    .title = { COPY(from_path, "setDesktopBackground.title") }
set-desktop-background-accept =
    .label = { COPY(from_path, "setDesktopBackground.title") }
open-desktop-prefs =
    .label = { COPY(from_path, "openDesktopPrefs.label") }
set-background-preview-unavailable = { COPY(from_path, "previewUnavailable") }
set-background-span =
    .label = { COPY(from_path, "span.label") }
set-background-color = { COPY(from_path, "color.label") }
set-background-position = { COPY(from_path, "position.label") }
set-background-tile =
    .label = { COPY(from_path, "tile.label") }
set-background-center =
    .label = { COPY(from_path, "center.label") }
set-background-stretch =
    .label = { COPY(from_path, "stretch.label") }
set-background-fill =
    .label = { COPY(from_path, "fill.label") }
set-background-fit =
    .label = { COPY(from_path, "fit.label") }
""", from_path="browser/chrome/browser/setDesktopBackground.dtd"))
