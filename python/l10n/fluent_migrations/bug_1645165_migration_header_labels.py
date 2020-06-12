# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1645165 - fix header labels in the browser data migration dialog, part {index}."""

    ctx.add_transforms(
        "browser/browser/migration.ftl",
        "browser/browser/migration.ftl",
        transforms_from("""
import-source-page-title = { COPY_PATTERN(from_path, "import-source.label") }
import-items-page-title = { COPY_PATTERN(from_path, "import-items-title.label") }
import-migrating-page-title = { COPY_PATTERN(from_path, "import-migrating-title.label") }
import-select-profile-page-title = { COPY_PATTERN(from_path, "import-select-profile-title.label") }
import-done-page-title = { COPY_PATTERN(from_path, "import-done-title.label") }
""", from_path="browser/browser/migration.ftl"))
