# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY


def migrate(ctx):
    """Bug 1682022 - Forking some misc AppMenu strings for sentence casing, part {index}."""
    ctx.add_transforms(
        "browser/browser/appmenu.ftl",
        "browser/browser/appmenu.ftl",
        transforms_from(
            """
appmenu-developer-tools-extensions =
    .label = { COPY(from_path, "extensionsForDevelopersCmd.label") }
""",
            from_path="devtools/client/menus.properties",
        ),
    )

    ctx.add_transforms(
        "browser/browser/appmenu.ftl",
        "browser/browser/appmenu.ftl",
        transforms_from(
            """
appmenuitem-new-window =
    .label = { COPY(from_path, "newNavigatorCmd.label") }
appmenuitem-new-private-window =
    .label = { COPY(from_path, "newPrivateWindow.label") }
appmenu-restore-session =
    .label = { COPY(from_path, "appMenuHistory.restoreSession.label") }
appmenuitem-fullscreen =
    .label = { COPY(from_path, "fullScreenCmd.label") }
appmenuitem-new-tab =
    .label = { COPY(from_path, "tabCmd.label") }
appmenu-clear-history =
    .label = { COPY(from_path, "appMenuHistory.clearRecent.label") }
appmenu-recent-history-subheader = { COPY(from_path, "appMenuHistory.recentHistory.label") }
appmenu-recently-closed-tabs =
    .label = { COPY(from_path, "historyUndoMenu.label") }
appmenu-recently-closed-windows =
    .label = { COPY(from_path, "historyUndoWindowMenu.label") }
""",
            from_path="browser/chrome/browser/browser.dtd",
        ),
    )
