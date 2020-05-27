# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from

def migrate(ctx):
    """Bug 1609556 - Migrate toolbar-context-menu to Fluent, part {index}."""

    ctx.add_transforms(
        "browser/browser/toolbarContextMenu.ftl",
        "browser/browser/toolbarContextMenu.ftl",
    transforms_from(
"""
toolbar-context-menu-manage-extension =
    .label = { COPY(from_path, "customizeMenu.manageExtension.label") }
    .accesskey = { COPY(from_path, "customizeMenu.manageExtension.accesskey") }
toolbar-context-menu-remove-extension =
    .label = { COPY(from_path, "customizeMenu.removeExtension.label") }
    .accesskey = { COPY(from_path, "customizeMenu.removeExtension.accesskey") }
toolbar-context-menu-report-extension =
    .label = { COPY(from_path, "customizeMenu.reportExtension.label") }
    .accesskey = { COPY(from_path, "customizeMenu.reportExtension.accesskey") }
toolbar-context-menu-pin-to-overflow-menu =
    .label = { COPY(from_path, "customizeMenu.pinToOverflowMenu.label") }
    .accesskey = { COPY(from_path, "customizeMenu.pinToOverflowMenu.accesskey") }
toolbar-context-menu-auto-hide-downloads-button =
    .label = { COPY(from_path, "customizeMenu.autoHideDownloadsButton.label") }
    .accesskey = { COPY(from_path, "customizeMenu.autoHideDownloadsButton.accesskey") }
toolbar-context-menu-remove-from-toolbar =
    .label = { COPY(from_path, "customizeMenu.removeFromToolbar.label") }
    .accesskey = { COPY(from_path, "customizeMenu.removeFromToolbar.accesskey") }
toolbar-context-menu-view-customize-toolbar =
    .label = { COPY(from_path, "viewCustomizeToolbar.label") }
    .accesskey = { COPY(from_path, "viewCustomizeToolbar.accesskey") }
""", from_path="browser/chrome/browser/browser.dtd"))

    ctx.add_transforms(
        "browser/browser/appmenu.ftl",
        "browser/browser/appmenu.ftl",
    transforms_from(
"""
appmenuitem-customize-mode =
    .label = { COPY(from_path, "viewCustomizeToolbar.label") }
""", from_path="browser/chrome/browser/browser.dtd"))
