# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1856014 - Fix fluent schema for strings in protections panel, part {index}."""

    file = "browser/browser/protectionsPanel.ftl"
    ctx.add_transforms(
        file,
        file,
        transforms_from(
            """
protections-panel-not-blocking-why-etp-on-tooltip-label =
  .label = {COPY_PATTERN(from_path, "protections-panel-not-blocking-why-etp-on-tooltip")}
protections-panel-not-blocking-why-etp-off-tooltip-label =
  .label = {COPY_PATTERN(from_path, "protections-panel-not-blocking-why-etp-off-tooltip")}
""",
            from_path=file,
        ),
    )
    ctx.add_transforms(
        file,
        file,
        transforms_from(
            """
protections-panel-cookie-banner-view-turn-off-label =
  .label = {COPY_PATTERN(from_path, "protections-panel-cookie-banner-view-turn-off")}
protections-panel-cookie-banner-view-turn-on-label =
  .label = {COPY_PATTERN(from_path, "protections-panel-cookie-banner-view-turn-on")}
""",
            from_path=file,
        ),
    )
