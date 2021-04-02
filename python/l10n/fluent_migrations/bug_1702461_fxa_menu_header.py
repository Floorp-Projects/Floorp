# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1702461, set fxa submenu header correctly when opening the panel, part {index}"""
    ctx.add_transforms(
        "browser/browser/appmenu.ftl",
        "browser/browser/appmenu.ftl",
        transforms_from(
            """
appmenu-fxa-header2 = { COPY_PATTERN(from_path, "appmenu-fxa-header.title") }
""",
            from_path="browser/browser/appmenu.ftl",
        ),
    )
