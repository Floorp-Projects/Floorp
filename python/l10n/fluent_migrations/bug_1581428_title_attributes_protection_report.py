# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY_PATTERN

TARGET_FILE = "browser/browser/protections.ftl"
SOURCE_FILE = TARGET_FILE

def migrate(ctx):
    """Bug 1581428 - Show tooltips for links in Protection Report, part {index}."""

    ctx.add_transforms(
        TARGET_FILE,
        SOURCE_FILE,
        transforms_from(
            """
protection-report-header-details-standard = {COPY_PATTERN(from_path, "protection-header-details-standard")}
    .title = {COPY_PATTERN(from_path, "go-to-privacy-settings")}
protection-report-header-details-strict = {COPY_PATTERN(from_path, "protection-header-details-strict")}
    .title = {COPY_PATTERN(from_path, "go-to-privacy-settings")}
protection-report-header-details-custom = {COPY_PATTERN(from_path, "protection-header-details-custom")}
    .title = {COPY_PATTERN(from_path, "go-to-privacy-settings")}
protection-report-view-logins-button = {COPY_PATTERN(from_path, "about-logins-view-logins-button")}
    .title = {COPY_PATTERN(from_path, "go-to-saved-logins")}
""",
            from_path=SOURCE_FILE),
    )
