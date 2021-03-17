# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1691140, add Task Manager to browser tools menu for proton, part {index}."""
    ctx.add_transforms(
        "browser/browser/appmenu.ftl",
        "browser/browser/appmenu.ftl",
        transforms_from(
            """
appmenu-taskmanager =
    .label = { COPY_PATTERN(from_path, "appmenu-help-taskmanager.label") }
""",
            from_path="browser/browser/appmenu.ftl",
        ),
    )
