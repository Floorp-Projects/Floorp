# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from

def migrate(ctx):
    """Bug 1609555 - Migrate sidebar menu to Fluent, part {index}."""

    ctx.add_transforms(
        "browser/browser/sidebarMenu.ftl",
        "browser/browser/sidebarMenu.ftl",
    transforms_from(
"""
sidebar-menu-bookmarks =
    .label = { COPY(from_path, "bookmarksButton.label") }
sidebar-menu-history =
    .label = { COPY(from_path, "historyButton.label") }
sidebar-menu-synced-tabs =
    .label = { COPY(from_path, "syncedTabs.sidebar.label") }
sidebar-menu-close =
    .label = { COPY(from_path, "sidebarMenuClose.label") }
""", from_path="browser/chrome/browser/browser.dtd"))
