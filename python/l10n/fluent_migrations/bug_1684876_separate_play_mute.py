# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY


def migrate(ctx):
    """Bug 1684876 - Migrate Play Tab and Play Tabs to Fluent, part {index}"""

    target = "browser/browser/tabContextMenu.ftl"
    reference = "browser/browser/tabContextMenu.ftl"

    ctx.add_transforms(
        target,
        reference,
        transforms_from(
            """
tab-context-play-tab =
    .label = { COPY(from_path, "playTab.label") }
    .accesskey = { COPY(from_path, "playTab.accesskey") }
""",
            from_path="browser/chrome/browser/browser.properties",
        ),
    )

    ctx.add_transforms(
        target,
        reference,
        transforms_from(
            """
tab-context-play-tabs =
    .label = { COPY(from_path, "playTabs.label") }
    .accesskey = { COPY(from_path, "playTabs.accesskey") }
""",
            from_path="browser/chrome/browser/browser.properties",
        ),
    )
