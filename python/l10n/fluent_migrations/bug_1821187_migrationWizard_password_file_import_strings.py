# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate import COPY_PATTERN
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1821187 - Copy password file import strings to migrationWizard.ftl, part {index}."""

    ctx.add_transforms(
        "browser/browser/migrationWizard.ftl",
        "browser/browser/migrationWizard.ftl",
        transforms_from(
            """
migration-passwords-from-file-csv-filter-title =
    {COPY_PATTERN(from_path, "about-logins-import-file-picker-csv-filter-title")}
migration-passwords-from-file-tsv-filter-title =
    {COPY_PATTERN(from_path, "about-logins-import-file-picker-tsv-filter-title")}
    """,
            from_path="browser/browser/aboutLogins.ftl",
        ),
    )
