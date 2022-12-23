# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from fluent.migrate import COPY_PATTERN
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1790387 - A11y improvements for the Feature Callout step indicator, part {index}"""
    ctx.add_transforms(
        "browser/browser/newtab/onboarding.ftl",
        "browser/browser/newtab/onboarding.ftl",
        transforms_from(
            """
onboarding-welcome-steps-indicator-label =
    .aria-label = { COPY_PATTERN(from_path, "onboarding-welcome-steps-indicator2.aria-valuetext") }
""",
            from_path="browser/browser/newtab/onboarding.ftl",
        ),
    )
