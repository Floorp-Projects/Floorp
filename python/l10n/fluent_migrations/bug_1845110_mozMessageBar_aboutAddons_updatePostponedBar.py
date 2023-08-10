# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1845110 - Use moz-message-bar to replace the postponed update message in about:addons, part {index}."""
    abouAddons_ftl = "toolkit/toolkit/about/aboutAddons.ftl"
    ctx.add_transforms(
        abouAddons_ftl,
        abouAddons_ftl,
        transforms_from(
            """
install-postponed-message2 =
    .message = {COPY_PATTERN(from_path, "install-postponed-message")}
""",
            from_path=abouAddons_ftl,
        ),
    )
