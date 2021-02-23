# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY_PATTERN


def migrate(ctx):
    """Bug 1688960 - Use h1 elements for .panel-header's , part {index}."""
    ctx.add_transforms(
        "browser/browser/browser.ftl",
        "browser/browser/browser.ftl",
        transforms_from(
            """
bookmarks-recent-bookmarks-panel-subheader = { COPY_PATTERN(from_path, "bookmarks-recent-bookmarks-panel.value") }
""",
            from_path="browser/browser/browser.ftl",
        ),
    )
