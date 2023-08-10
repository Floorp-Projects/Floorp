# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1845109 - Use moz-message-bar to replace the addon-card-message in about:addons, part {index}."""
    abouAddons_ftl = "toolkit/toolkit/about/aboutAddons.ftl"
    ctx.add_transforms(
        abouAddons_ftl,
        abouAddons_ftl,
        transforms_from(
            """
details-notification-incompatible2 =
    .message = {COPY_PATTERN(from_path, "details-notification-incompatible")}

details-notification-unsigned-and-disabled2 =
    .message = {COPY_PATTERN(from_path, "details-notification-unsigned-and-disabled")}

details-notification-unsigned2 =
    .message = {COPY_PATTERN(from_path, "details-notification-unsigned")}

details-notification-blocked2 =
    .message = {COPY_PATTERN(from_path, "details-notification-blocked")}

details-notification-softblocked2 =
    .message = {COPY_PATTERN(from_path, "details-notification-softblocked")}

details-notification-gmp-pending2 =
    .message = {COPY_PATTERN(from_path, "details-notification-gmp-pending")}
""",
            from_path=abouAddons_ftl,
        ),
    )
