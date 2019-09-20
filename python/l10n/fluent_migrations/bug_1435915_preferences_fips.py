# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import VARIABLE_REFERENCE, TERM_REFERENCE
from fluent.migrate import REPLACE, COPY


def migrate(ctx):
    """Bug 1435915 - Migrate Preferences Password FIPS dialog, part {index}."""

    ctx.add_transforms(
        "browser/browser/preferences/preferences.ftl",
        "browser/browser/preferences/preferences.ftl",
        transforms_from(
            """
forms-master-pw-fips-title = { COPY(from_path, "pw_change2empty_in_fips_mode") }
forms-master-pw-fips-desc = { COPY(from_path, "pw_change_failed_title") }
""", from_path="browser/chrome/browser/preferences/preferences.properties"))
