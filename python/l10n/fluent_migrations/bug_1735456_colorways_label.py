# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY_PATTERN


def migrate(ctx):
    """Bug 1735456 - about:welcome: Theme radio buttons have no text in NVDA browse mode, part {index}"""
    ctx.add_transforms(
        "browser/browser/newtab/onboarding.ftl",
        "browser/browser/newtab/onboarding.ftl",
        transforms_from(
            """
mr2-onboarding-colorway-label =
    { COPY_PATTERN(from_path, "mr2-onboarding-colorway-description.aria-description") }
mr2-onboarding-default-theme-label =
    { COPY_PATTERN(from_path, "mr2-onboarding-default-theme-description.aria-description") }
""",
            from_path="browser/browser/newtab/onboarding.ftl",
        ),
    )
