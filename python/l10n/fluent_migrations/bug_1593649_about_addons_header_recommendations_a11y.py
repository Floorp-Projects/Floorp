# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY_PATTERN

TARGET_FILE = "toolkit/toolkit/about/aboutAddons.ftl"
SOURCE_FILE = TARGET_FILE


def migrate(ctx):
    """Bug 1593649 - about:addons tools button, part {index}"""

    ctx.add_transforms(
        TARGET_FILE,
        SOURCE_FILE,
        transforms_from(
"""
addon-page-options-button =
    .title = {COPY_PATTERN(from_path, "tools-menu.tooltiptext")}
addons-page-title = {COPY_PATTERN(from_path, "addons-window.title")}
""",
        from_path=SOURCE_FILE),
    )
