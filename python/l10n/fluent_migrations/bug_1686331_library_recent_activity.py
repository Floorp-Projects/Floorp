# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY_PATTERN


def migrate(ctx):
    """Bug 1686331 - Library menu should not have a scroll bar part {index}."""
    ctx.add_transforms(
        "browser/browser/browser.ftl",
        "browser/browser/browser.ftl",
        transforms_from(
            """
library-recent-activity-title =
    .value = {COPY_PATTERN(from_path, "library-recent-activity-label")}
""",
            from_path="browser/browser/browser.ftl",
        ),
    )
