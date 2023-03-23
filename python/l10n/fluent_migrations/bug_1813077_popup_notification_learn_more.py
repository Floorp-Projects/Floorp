# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1813077 - Migrate xpinstallPromptMessage.learnMore to Fluent , part {index}."""

    ctx.add_transforms(
        "browser/browser/browser.ftl",
        "browser/browser/browser.ftl",
        transforms_from(
            """
popup-notification-xpinstall-prompt-learn-more = { COPY("browser/chrome/browser/browser.properties", "xpinstallPromptMessage.learnMore") }
"""
        ),
    )
