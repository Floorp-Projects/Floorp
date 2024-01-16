# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1866295 - Land new strings for device migration ASRouter messages, part {index}."""

    source = "browser/browser/newtab/asrouter.ftl"
    target = source

    ctx.add_transforms(
        target,
        target,
        transforms_from(
            """
device-migration-fxa-spotlight-getting-new-device-primary-button = {COPY_PATTERN(from_path, "device-migration-fxa-spotlight-primary-button")}
""",
            from_path=source,
        ),
    )
