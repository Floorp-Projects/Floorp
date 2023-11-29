# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1867346 - Replace a string for device migration ASRouter messages, part {index}."""

    source = "browser/browser/newtab/asrouter.ftl"
    target = source

    ctx.add_transforms(
        target,
        target,
        transforms_from(
            """
device-migration-fxa-spotlight-getting-new-device-header-2 = {COPY_PATTERN(from_path, "fxa-sync-cfr-header")}
""",
            from_path=source,
        ),
    )
