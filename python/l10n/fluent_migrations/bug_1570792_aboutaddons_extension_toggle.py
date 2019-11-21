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
    """Bug 1570792 - Add a toggle to extension cards, part {index}"""

    ctx.add_transforms(
        TARGET_FILE,
        SOURCE_FILE,
        transforms_from(
"""
disable-addon-button-label =
    .aria-label = {COPY_PATTERN(from_path, "disable-addon-button")}
enable-addon-button-label =
    .aria-label = {COPY_PATTERN(from_path, "enable-addon-button")}
""",
        from_path=SOURCE_FILE),
    )
