# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1689366 - simplify bookmark panel, part {index}."""
    ctx.add_transforms(
        "browser/browser/browser.ftl",
        "browser/browser/browser.ftl",
        transforms_from(
            """
bookmarks-bookmark-edit-panel =
    .label = { COPY_PATTERN(from_path, "library-bookmarks-bookmark-edit.label") }
bookmarks-tools-toolbar-visibility-menuitem =
    .label = { COPY_PATTERN(from_path, "bookmarks-tools-toolbar-visibility.label") }
""",
            from_path="browser/browser/browser.ftl",
        ),
    )
