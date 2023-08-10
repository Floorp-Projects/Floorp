# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1845113 - Use moz-message-bar for the recommended extensions message in about:addons, part {index}."""
    abouAddons_ftl = "toolkit/toolkit/about/aboutAddons.ftl"
    ctx.add_transforms(
        abouAddons_ftl,
        abouAddons_ftl,
        transforms_from(
            """
discopane-notice-recommendations2 =
    .message = {COPY_PATTERN(from_path, "discopane-notice-recommendations")}
""",
            from_path=abouAddons_ftl,
        ),
    )
