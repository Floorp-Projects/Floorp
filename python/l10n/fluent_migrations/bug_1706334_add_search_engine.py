# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1706334 - Improving accessible text on Add Engine items, part {index}."""
    ctx.add_transforms(
        "browser/browser/browser.ftl",
        "browser/browser/browser.ftl",
        transforms_from(
            """
search-one-offs-add-engine-menu =
    .label = { COPY(from_path, "cmd_addFoundEngineMenu") }
""",
            from_path="browser/chrome/browser/search.properties",
        ),
    )
