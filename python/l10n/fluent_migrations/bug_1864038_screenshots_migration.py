# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1864038 - Consolidate screenshots fluent files, part {index}."""

    source = "browser/browser/screenshotsOverlay.ftl"
    target = "browser/browser/screenshots.ftl"

    ctx.add_transforms(
        target,
        target,
        transforms_from(
            """
screenshots-component-copy-button-label = {COPY_PATTERN(from_path, "screenshots-overlay-copy-button")}

screenshots-component-download-button-label = {COPY_PATTERN(from_path, "screenshots-overlay-download-button")}

screenshots-overlay-selection-region-size-2 = {COPY_PATTERN(from_path, "screenshots-overlay-selection-region-size")}
""",
            from_path=source,
        ),
    )

    ctx.add_transforms(
        target,
        target,
        transforms_from(
            """
screenshots-component-retry-button =
    .title = {COPY_PATTERN(from_path, "screenshots-retry-button-title.title")}
    .aria-label = {COPY_PATTERN(from_path, "screenshots-retry-button-title.title")}
""",
            from_path=target,
        ),
    )
