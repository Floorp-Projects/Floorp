# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY_PATTERN

TARGET_FILE = "toolkit/toolkit/about/aboutSupport.ftl"
SOURCE_FILE = TARGET_FILE


def migrate(ctx):
    """Bug 1588925 - Fix broken identifier for about:support d3d9 video crash guard string, part {index}"""


    ctx.add_transforms(
        TARGET_FILE,
        SOURCE_FILE,
        transforms_from(
"""
d3d9video-crash-guard = {COPY_PATTERN(from_path, "d3d9video-crash-buard")}
""",
        from_path=SOURCE_FILE),
    )
