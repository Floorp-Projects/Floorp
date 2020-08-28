# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1660259 - Correct nav aria-label and label and description for theme buttons, part {index}."""

    ctx.add_transforms(
        "browser/browser/newtab/onboarding.ftl",
        "browser/browser/newtab/onboarding.ftl",
        transforms_from("""
onboarding-multistage-theme-tooltip-automatic-2 =
   .title = { COPY_PATTERN(from_path, "onboarding-multistage-theme-tooltip-automatic.title") }
onboarding-multistage-theme-description-automatic-2 =
  .aria-description =  { COPY_PATTERN(from_path, "onboarding-multistage-theme-tooltip-automatic.title") }

onboarding-multistage-theme-tooltip-light-2 =
   .title = { COPY_PATTERN(from_path, "onboarding-multistage-theme-tooltip-light.title") }
onboarding-multistage-theme-description-light =
  .aria-description = { COPY_PATTERN(from_path, "onboarding-multistage-theme-tooltip-light.title") }

onboarding-multistage-theme-tooltip-dark-2 =
   .title = { COPY_PATTERN(from_path, "onboarding-multistage-theme-tooltip-dark.title") }
onboarding-multistage-theme-description-dark =
  .aria-description = { COPY_PATTERN(from_path, "onboarding-multistage-theme-tooltip-dark.title") }

onboarding-multistage-theme-tooltip-alpenglow-2 =
   .title = { COPY_PATTERN(from_path, "onboarding-multistage-theme-tooltip-alpenglow.title") }
onboarding-multistage-theme-description-alpenglow =
  .aria-description = { COPY_PATTERN(from_path, "onboarding-multistage-theme-tooltip-alpenglow.title") }
""", from_path="browser/browser/newtab/onboarding.ftl"))
