# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1845148 - Use moz-message-bar for global warnings in about:addons, part {index}."""
    aboutAddons_ftl = "toolkit/toolkit/about/aboutAddons.ftl"
    ctx.add_transforms(
        aboutAddons_ftl,
        aboutAddons_ftl,
        transforms_from(
            """
extensions-warning-safe-mode2 =
    .message = {COPY_PATTERN(from_path, "extensions-warning-safe-mode")}

extensions-warning-check-compatibility2 =
    .message = {COPY_PATTERN(from_path, "extensions-warning-check-compatibility")}

extensions-warning-update-security2 =
    .message = {COPY_PATTERN(from_path, "extensions-warning-update-security")}

extensions-warning-imported-addons2 =
    .message = {COPY_PATTERN(from_path, "extensions-warning-imported-addons")}
""",
            from_path=aboutAddons_ftl,
        ),
    )
