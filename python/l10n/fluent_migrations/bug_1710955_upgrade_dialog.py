# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1710955 - Change MR1 upgrade onboarding to Pin then Default then Theme screens, part {index}"""

    ctx.add_transforms(
        "browser/browser/upgradeDialog.ftl",
        "browser/browser/upgradeDialog.ftl",
        transforms_from(
            """
upgrade-dialog-new-alt-subtitle = { COPY_PATTERN(from_path, "onboarding-multistage-pin-default-header") }
            """,
            from_path="browser/browser/newtab/onboarding.ftl",
        ),
    )

    ctx.add_transforms(
        "browser/browser/upgradeDialog.ftl",
        "browser/browser/upgradeDialog.ftl",
        transforms_from(
            """
upgrade-dialog-default-title = { COPY_PATTERN(from_path, "default-browser-prompt-title-alt") }
upgrade-dialog-default-subtitle = { COPY_PATTERN(from_path, "default-browser-prompt-message-alt") }
upgrade-dialog-default-primary-button = { COPY_PATTERN(from_path, "default-browser-prompt-button-primary-alt") }
upgrade-dialog-default-secondary-button = { COPY_PATTERN(from_path, "default-browser-prompt-button-secondary") }
            """,
            from_path="browser/browser/defaultBrowserNotification.ftl",
        ),
    )
