# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/


from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY


def migrate(ctx):
    """Bug 1832668 - Add new side navigation component to Firefox View Next page, part {index}."""
    ctx.add_transforms(
        "browser/browser/firefoxView.ftl",
        "browser/browser/firefoxView.ftl",
        transforms_from(
            """
firefoxview-overview-nav = {COPY_PATTERN(from_path, "firefoxview-overview-navigation")}
   .title = {COPY_PATTERN(from_path, "firefoxview-overview-navigation")}
firefoxview-history-nav = {COPY_PATTERN(from_path, "firefoxview-history-navigation")}
   .title = {COPY_PATTERN(from_path, "firefoxview-history-navigation")}
firefoxview-opentabs-nav = {COPY_PATTERN(from_path, "firefoxview-opentabs-navigation")}
   .title = {COPY_PATTERN(from_path, "firefoxview-opentabs-navigation")}
    """,
            from_path="browser/browser/firefoxView.ftl",
        ),
    )
