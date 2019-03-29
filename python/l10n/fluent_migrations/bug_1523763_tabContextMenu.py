
# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import MESSAGE_REFERENCE, TERM_REFERENCE
from fluent.migrate import COPY


def migrate(ctx):
    """Bug 15237631 - Convert tab context menu strings from browser.dtd to Fluent, part {index}."""

    ctx.add_transforms(
        "browser/browser/tabContextMenu.ftl",
        "browser/browser/tabContextMenu.ftl",
        transforms_from(
"""
reload-tab =
    .label = { COPY("browser/chrome/browser/browser.dtd","reloadTab.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd","reloadTab.accesskey" ) }
select-all-tabs =
    .label = { COPY("browser/chrome/browser/browser.dtd","selectAllTabs.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd","selectAllTabs.accesskey") }
duplicate-tab =
    .label = { COPY("browser/chrome/browser/browser.dtd","duplicateTab.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd","duplicateTab.accesskey") }
duplicate-tabs =
    .label = { COPY("browser/chrome/browser/browser.dtd","duplicateTabs.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd","duplicateTabs.accesskey") }
close-tabs-to-the-end =
    .label = { COPY("browser/chrome/browser/browser.dtd","closeTabsToTheEnd.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd","closeTabsToTheEnd.accesskey") }
close-other-tabs =
    .label = { COPY("browser/chrome/browser/browser.dtd","closeOtherTabs.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd","closeOtherTabs.accesskey") }
reload-tabs =
    .label = { COPY("browser/chrome/browser/browser.dtd","reloadTabs.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd","reloadTabs.accesskey") }
pin-tab =
    .label = { COPY("browser/chrome/browser/browser.dtd","pinTab.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd","pinTab.accesskey") }
unpin-tab =
    .label = { COPY("browser/chrome/browser/browser.dtd","unpinTab.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd","unpinTab.accesskey") }
pin-selected-tabs =
    .label = { COPY("browser/chrome/browser/browser.dtd","pinSelectedTabs.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd","pinSelectedTabs.accesskey") }
unpin-selected-tabs = 
    .label = { COPY("browser/chrome/browser/browser.dtd","unpinSelectedTabs.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd","unpinSelectedTabs.accesskey") }
bookmark-selected-tabs =
    .label = { COPY("browser/chrome/browser/browser.dtd","bookmarkSelectedTabs.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd","bookmarkSelectedTabs.accesskey") }
bookmark-tab =
    .label = { COPY("browser/chrome/browser/browser.dtd","bookmarkTab.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd","bookmarkTab.accesskey") }
reopen-in-container =
    .label = { COPY("browser/chrome/browser/browser.dtd","reopenInContainer.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd","reopenInContainer.accesskey") }
move-to-start =
    .label = { COPY("browser/chrome/browser/browser.dtd","moveToStart.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd","moveToStart.accesskey") }
move-to-end =
    .label = { COPY("browser/chrome/browser/browser.dtd","moveToEnd.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd","moveToEnd.accesskey") }
move-to-new-window =
    .label = { COPY("browser/chrome/browser/browser.dtd","moveToNewWindow.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd","moveToNewWindow.accesskey") }
undo-close-tab =
    .label = { COPY("browser/chrome/browser/browser.dtd","undoCloseTab.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd","undoCloseTab.accesskey") }
close-tab =
    .label = { COPY("browser/chrome/browser/browser.dtd","closeTab.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd","closeTab.accesskey") }
close-tabs =
    .label = { COPY("browser/chrome/browser/browser.dtd","closeTabs.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd","closeTabs.accesskey") }
move-tabs =
    .label = { COPY("browser/chrome/browser/browser.dtd","moveSelectedTabOptions.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd","moveSelectedTabOptions.accesskey") }
move-tab =
    .label = { COPY("browser/chrome/browser/browser.dtd","moveTabOptions.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd","moveTabOptions.accesskey") }
""")
)
