# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1682022 - Fork more strings from the context menu and browser.dtd for the AppMenu, part {index}."""
    ctx.add_transforms(
        "browser/browser/appmenu.ftl",
        "browser/browser/appmenu.ftl",
        transforms_from(
            """
appmenuitem-save-page =
  .label = { COPY_PATTERN(from_path, "main-context-menu-page-save.label") }
""",
            from_path="browser/browser/browserContext.ftl",
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
appmenuitem-fullscreen =
  .label = { COPY(from_path, "fullScreenCmd.label") }
""",
            from_path="browser/chrome/browser/browser.dtd",
        ),
    )
