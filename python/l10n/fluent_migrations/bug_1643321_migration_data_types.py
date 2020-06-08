# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1643321 - reorganize data type strings for migration from other browsers, part {index}."""

    ctx.add_transforms(
        "browser/browser/migration.ftl",
        "browser/browser/migration.ftl",
        transforms_from("""
browser-data-session-checkbox =
    .label = { COPY_PATTERN(from_path, "browser-data-firefox-128.label") }
browser-data-session-label =
    .value = { COPY_PATTERN(from_path, "browser-data-firefox-128.value") }

""", from_path="browser/browser/migration.ftl"))
