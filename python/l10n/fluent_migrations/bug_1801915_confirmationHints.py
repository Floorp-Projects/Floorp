# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import VARIABLE_REFERENCE
from fluent.migrate import COPY, REPLACE


def migrate(ctx):
    """Bug 1801915 - Migrate confirmation Hints to Fluent"""
    ctx.add_transforms(
        "browser/browser/confirmationHints.ftl",
        "browser/browser/confirmationHints.ftl",
        transforms_from(
            """
confirmation-hint-breakage-report-sent = { COPY(from_path, "confirmationHint.breakageReport.label") }
confirmation-hint-login-removed = { COPY(from_path, "confirmationHint.loginRemoved.label") }
confirmation-hint-page-bookmarked = { COPY(from_path, "confirmationHint.pageBookmarked2.label") }
confirmation-hint-password-saved = { COPY(from_path, "confirmationHint.passwordSaved.label") }
confirmation-hint-pin-tab = { COPY(from_path, "confirmationHint.pinTab.label") }
confirmation-hint-pin-tab-description = { COPY(from_path, "confirmationHint.pinTab.description") }
confirmation-hint-send-to-device = { COPY(from_path, "confirmationHint.sendToDevice.label") }
""",
            from_path="browser/chrome/browser/browser.properties",
        ),
    )
