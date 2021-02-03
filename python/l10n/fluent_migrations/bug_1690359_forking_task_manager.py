# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1690359 - Move task manager to help menu, part {index}."""

    ctx.add_transforms(
        "browser/browser/menubar.ftl",
        "browser/browser/menubar.ftl",
        transforms_from(
            """
menu-help-taskmanager =
    .label = { COPY(from_path, "taskManagerCmd.label") }
""",
            from_path="browser/chrome/browser/browser.dtd",
        ),
    )
    ctx.add_transforms(
        "browser/browser/appmenu.ftl",
        "browser/browser/appmenu.ftl",
        transforms_from(
            """
appmenu-help-taskmanager =
    .label = { COPY(from_path, "taskManagerCmd.label") }
""",
            from_path="browser/chrome/browser/browser.dtd",
        ),
    )
