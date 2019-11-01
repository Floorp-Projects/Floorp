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
    """Bug 1525175 - Move about:addons global warnings to HTML browser, part {index}"""

    ctx.add_transforms(
        TARGET_FILE,
        SOURCE_FILE,
        transforms_from(
"""
extensions-warning-safe-mode = {COPY_PATTERN(from_path, "extensions-warning-safe-mode-label.value")}
extensions-warning-check-compatibility = {COPY_PATTERN(from_path, "extensions-warning-check-compatibility-label.value")}
extensions-warning-check-compatibility-button = {COPY_PATTERN(from_path, "extensions-warning-check-compatibility-enable.label")}
    .title = {COPY_PATTERN(from_path, "extensions-warning-check-compatibility-enable.tooltiptext")}
extensions-warning-update-security = {COPY_PATTERN(from_path, "extensions-warning-update-security-label.value")}
extensions-warning-update-security-button = {COPY_PATTERN(from_path, "extensions-warning-update-security-enable.label")}
    .title = {COPY_PATTERN(from_path, "extensions-warning-update-security-enable.tooltiptext")}
""",
        from_path=SOURCE_FILE),
    )
