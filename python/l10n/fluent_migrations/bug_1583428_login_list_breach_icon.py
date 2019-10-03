# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY_PATTERN

TARGET_FILE = "browser/browser/aboutLogins.ftl"
SOURCE_FILE = TARGET_FILE

def migrate(ctx):
    """Bug 1583428 - Remove unnecessary alt attribute on breach icon, part {index}."""

    ctx.add_transforms(
        TARGET_FILE,
        SOURCE_FILE,
        transforms_from(
            """
about-logins-list-item-breach-icon =
    .title = {COPY_PATTERN(from_path, "about-logins-list-item-warning-icon.title")}
""",
            from_path=SOURCE_FILE),
    )
