# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1845123 - Use moz-message-bar to show extension shortcut warnings in about:addons, part {index}."""
    aboutAddons_ftl = "toolkit/toolkit/about/aboutAddons.ftl"
    ctx.add_transforms(
        aboutAddons_ftl,
        aboutAddons_ftl,
        transforms_from(
            """
shortcuts-duplicate-warning-message2 =
    .message = {COPY_PATTERN(from_path, "shortcuts-duplicate-warning-message")}
""",
            from_path=aboutAddons_ftl,
        ),
    )
