# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1892125 - Update copy and download strings, part {index}."""

    source = "browser/browser/screenshots.ftl"
    target = "browser/browser/screenshots.ftl"

    ctx.add_transforms(
        target,
        target,
        transforms_from(
            """
screenshots-component-copy-button-2 = {COPY_PATTERN(from_path, "screenshots-component-copy-button-label")}
    .title = {COPY_PATTERN(from_path, "screenshots-component-copy-button.title")}
    .aria-label = {COPY_PATTERN(from_path, "screenshots-component-copy-button.aria-label")}

screenshots-component-download-button-2 = {COPY_PATTERN(from_path, "screenshots-component-download-button-label")}
    .title = {COPY_PATTERN(from_path, "screenshots-component-download-button.title")}
    .aria-label = {COPY_PATTERN(from_path, "screenshots-component-download-button.aria-label")}
""",
            from_path=source,
        ),
    )
