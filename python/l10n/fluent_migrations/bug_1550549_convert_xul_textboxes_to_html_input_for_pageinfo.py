# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY_PATTERN

TARGET_FILE = "browser/browser/pageInfo.ftl"
SOURCE_FILE = TARGET_FILE


def migrate(ctx):
    """Bug 1550549 - Convert XUL textboxes to HTML inputs in pageInfo.xul, part {index}."""

    ctx.add_transforms(
        TARGET_FILE,
        SOURCE_FILE,
        transforms_from(
            """
page-info-not-specified =
    .value = {COPY_PATTERN(from_path, "not-set-verified-by")}
page-info-security-no-owner =
    .value = {COPY_PATTERN(from_path, "security-no-owner")}
""",
            from_path=SOURCE_FILE),
    )
