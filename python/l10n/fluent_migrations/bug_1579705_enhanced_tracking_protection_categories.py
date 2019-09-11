# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY_PATTERN

TARGET_FILE = "browser/browser/preferences/preferences.ftl"
SOURCE_FILE = TARGET_FILE


def migrate(ctx):
    """Bug 1579705 - Update level labels for Enhanced Tracking Protection, part {index}."""

    ctx.add_transforms(
        TARGET_FILE,
        SOURCE_FILE,
        transforms_from(
            """
enhanced-tracking-protection-setting-standard =
    .label = {COPY_PATTERN(from_path, "content-blocking-setting-standard.label")}
    .accesskey = {COPY_PATTERN(from_path, "content-blocking-setting-standard.accesskey")}
enhanced-tracking-protection-setting-strict =
    .label = {COPY_PATTERN(from_path, "content-blocking-setting-strict.label")}
    .accesskey = {COPY_PATTERN(from_path, "content-blocking-setting-strict.accesskey")}
enhanced-tracking-protection-setting-custom =
    .label = {COPY_PATTERN(from_path, "content-blocking-setting-custom.label")}
    .accesskey = {COPY_PATTERN(from_path, "content-blocking-setting-custom.accesskey")}
""",
            from_path=SOURCE_FILE),
    )
