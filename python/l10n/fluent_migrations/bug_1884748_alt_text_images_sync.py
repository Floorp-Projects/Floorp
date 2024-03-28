# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1884748 - Add alt text to images in about:preferences#sync, part {index}."""

    source = "browser/browser/preferences/preferences.ftl"
    target = source

    ctx.add_transforms(
        target,
        target,
        transforms_from(
            """
sync-profile-picture-with-alt =
    .tooltiptext = {COPY_PATTERN(from_path, "sync-profile-picture.tooltiptext")}
    .alt = {COPY_PATTERN(from_path, "sync-profile-picture.tooltiptext")}
""",
            from_path=source,
        ),
    )
