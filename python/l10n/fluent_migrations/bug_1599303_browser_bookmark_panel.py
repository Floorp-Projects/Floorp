# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY_PATTERN


def migrate(ctx):
    """Bug 1599303 - Migrate bookmark panel to Fluent, part {index}."""

    ctx.add_transforms(
        'browser/browser/browser.ftl',
        'browser/browser/browser.ftl',
        transforms_from(
"""
bookmark-panel-show-editor-checkbox =
    .label = { COPY(from_path, "editBookmark.showForNewBookmarks.label") }
    .accesskey = { COPY(from_path, "editBookmark.showForNewBookmarks.accesskey") }
bookmark-panel-done-button =
    .label = { COPY(from_path, "editBookmark.done.label") }
bookmark-panel =
    .style = min-width: { COPY(from_path, "editBookmark.panel.width") }
""", from_path="browser/chrome/browser/browser.dtd")
    )
