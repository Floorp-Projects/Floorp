# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY_PATTERN


def migrate(ctx):
    """Bug 1694678: update fxa and remote tabs sync now buttons, part {index}."""
    ctx.add_transforms(
        "browser/browser/sync.ftl",
        "browser/browser/sync.ftl",
        transforms_from(
            """
fxa-toolbar-sync-syncing2 = { COPY_PATTERN(from_path, "fxa-toolbar-sync-syncing.label") }
""",
            from_path="browser/browser/sync.ftl",
        ),
    )

    ctx.add_transforms(
        "browser/browser/appmenu.ftl",
        "browser/browser/appmenu.ftl",
        transforms_from(
            """
appmenuitem-fxa-toolbar-sync-now2 = { COPY_PATTERN(from_path, "appmenuitem-fxa-toolbar-sync-now.label") }
""",
            from_path="browser/browser/appmenu.ftl",
        ),
    )
