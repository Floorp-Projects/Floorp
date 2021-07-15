# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY_PATTERN


def migrate(ctx):
    """Bug 1719727 - Change "Personalize" on New Tab to gear icon, part {index}"""
    ctx.add_transforms(
        "browser/browser/newtab/newtab.ftl",
        "browser/browser/newtab/newtab.ftl",
        transforms_from(
            """
newtab-personalize-icon-label =
    .title = { COPY_PATTERN(from_path, "newtab-personalize-button-label.title") }
    .aria-label = { COPY_PATTERN(from_path, "newtab-personalize-button-label.aria-label") }
""",
            from_path="browser/browser/newtab/newtab.ftl",
        ),
    )
