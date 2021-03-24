# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1697617 - Fork the Manage Account string from the App Menu for Synced Tabs, part {index}"""
    ctx.add_transforms(
        "browser/browser/browser.ftl",
        "browser/browser/browser.ftl",
        transforms_from(
            """
remote-tabs-manage-account =
  .label = { COPY_PATTERN(from_path, "appmenuitem-fxa-manage-account") }
remote-tabs-sync-now = { COPY_PATTERN(from_path, "appmenuitem-fxa-toolbar-sync-now2") }
""",
            from_path="browser/browser/appmenu.ftl",
        ),
    )
