# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY_PATTERN


def migrate(ctx):
    """Bug 1688960 - Moving save to Pocket button into toolbar, part {index}."""
    ctx.add_transforms(
        "browser/browser/browser.ftl",
        "browser/browser/browser.ftl",
        transforms_from(
            """
save-to-pocket-button =
    .label = { COPY_PATTERN(from_path, "urlbar-pocket-button.tooltiptext") }
    .tooltiptext = { COPY_PATTERN(from_path, "urlbar-pocket-button.tooltiptext") }
""",
            from_path="browser/browser/browser.ftl",
        ),
    )
