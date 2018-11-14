# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import VARIABLE_REFERENCE, TERM_REFERENCE
from fluent.migrate import REPLACE, COPY


def migrate(ctx):
    """Bug 1486937 - Move strings from preferences.properties to Fluent"""

    ctx.add_transforms(
        "toolkit/toolkit/preferences/preferences.ftl",
        "toolkit/toolkit/preferences/preferences.ftl",
        transforms_from(
"""
password-not-set =
        .value = { COPY(from_path, "password_not_set") }
failed-pw-change = { COPY(from_path, "failed_pw_change") }
incorrect-pw = { COPY(from_path, "incorrect_pw") }
pw-empty-warning = { COPY(from_path, "pw_empty_warning") }
pw-change-ok = { COPY(from_path, "pw_change_ok") } { -pw-empty-warning }
pw-erased-ok = { COPY(from_path, "pw_erased_ok") } { -pw-empty-warning }
pw-not-wanted = { COPY(from_path, "pw_not_wanted") }
pw-change2empty-in-fips-mode = { COPY(from_path, "pw_change2empty_in_fips_mode") }
pw-change-success-title = { COPY(from_path, "pw_change_success_title") }
pw-change-failed-title = { COPY(from_path, "pw_change_failed_title") }
pw-remove-button =
    .label = { COPY(from_path, "pw_remove_button") }
""", from_path="toolkit/chrome/mozapps/preferences/preferences.properties"))
