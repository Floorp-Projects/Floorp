# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.transforms import COPY_PATTERN


def migrate(ctx):
    """Bug 1853638 - Fix protections dashboard menu tooltip, part {index}."""

    file = "browser/browser/siteProtections.ftl"
    ctx.add_transforms(
        file,
        file,
        [
            FTL.Message(
                id=FTL.Identifier(
                    "protections-footer-blocked-tracker-counter-no-tooltip"
                ),
                value=COPY_PATTERN(file, "protections-footer-blocked-tracker-counter"),
            ),
        ],
    )
