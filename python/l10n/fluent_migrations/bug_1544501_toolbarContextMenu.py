# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import MESSAGE_REFERENCE, TERM_REFERENCE, VARIABLE_REFERENCE
from fluent.migrate import COPY, CONCAT, REPLACE

def migrate(ctx):
    """Bug 1544501 - Migrate toolbar context menu DTD strings from browser.dtd, part {index}"""
    ctx.add_transforms(
        "browser/browser/toolbarContextMenu.ftl",
        "browser/browser/toolbarContextMenu.ftl",
        transforms_from(
"""
toolbar-context-menu-reload-selected-tab =
    .label = { COPY(from_path,"toolbarContextMenu.reloadSelectedTab.label") }
    .accesskey = { COPY(from_path,"toolbarContextMenu.reloadSelectedTab.accesskey") }
toolbar-context-menu-reload-selected-tabs =
    .label = { COPY(from_path,"toolbarContextMenu.reloadSelectedTabs.label") }
    .accesskey = { COPY(from_path,"toolbarContextMenu.reloadSelectedTabs.accesskey") }
toolbar-context-menu-bookmark-selected-tab =
    .label = { COPY(from_path,"toolbarContextMenu.bookmarkSelectedTab.label") }
    .accesskey = { COPY(from_path,"toolbarContextMenu.bookmarkSelectedTab.accesskey") }
toolbar-context-menu-bookmark-selected-tabs =
    .label = { COPY(from_path,"toolbarContextMenu.bookmarkSelectedTabs.label") }
    .accesskey = { COPY(from_path,"toolbarContextMenu.bookmarkSelectedTabs.accesskey") }
toolbar-context-menu-select-all-tabs =
    .label = { COPY(from_path,"toolbarContextMenu.selectAllTabs.label") }
    .accesskey = { COPY(from_path,"toolbarContextMenu.selectAllTabs.accesskey") }
toolbar-context-menu-undo-close-tab =
    .label = { COPY(from_path,"toolbarContextMenu.undoCloseTab.label") }
    .accesskey = { COPY(from_path,"toolbarContextMenu.undoCloseTab.accesskey") }
""", from_path="browser/chrome/browser/browser.dtd"
        )
    )
