# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY_PATTERN


def migrate(ctx):
    """Bug 1698062: set app menu width and ensure text can wrap, part {index}."""
    ctx.add_transforms(
        "browser/browser/appmenu.ftl",
        "browser/browser/appmenu.ftl",
        transforms_from(
            """
appmenu-fxa-sync-and-save-data2 = { COPY_PATTERN(from_path, "appmenu-fxa-sync-and-save-data.value") }
""",
            from_path="browser/browser/appmenu.ftl",
        ),
    )
