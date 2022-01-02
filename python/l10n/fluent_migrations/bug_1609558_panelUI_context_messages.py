# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY


def migrate(ctx):
    """Bug 1609558 - Migrate panelUI.inc.xhtml to Fluent, part {index}."""
    ctx.add_transforms(
        "browser/browser/panelUI.ftl",
        "browser/browser/panelUI.ftl",
        transforms_from(
            """
customize-menu-unpin-from-overflowmenu =
    .label = { COPY(from_path, "customizeMenu.unpinFromOverflowMenu.label") }
    .accesskey = { COPY(from_path, "customizeMenu.unpinFromOverflowMenu.accesskey") }
customize-menu-add-to-toolbar =
    .label = { COPY(from_path, "customizeMenu.addToToolbar.label") }
    .accesskey = { COPY(from_path, "customizeMenu.addToToolbar.accesskey") }
customize-menu-add-to-overflowmenu =
    .label = { COPY(from_path, "customizeMenu.addToOverflowMenu.label") }
    .accesskey = { COPY(from_path, "customizeMenu.addToOverflowMenu.accesskey") }
panic-button-thankyou-msg1 = { COPY(from_path, "panicButton.thankyou.msg1") }
panic-button-thankyou-msg2 = { COPY(from_path, "panicButton.thankyou.msg2") }
panic-button-thankyou-button =
    .label = { COPY(from_path, "panicButton.thankyou.buttonlabel") }
""",
            from_path="browser/chrome/browser/browser.dtd",
        ),
    )
