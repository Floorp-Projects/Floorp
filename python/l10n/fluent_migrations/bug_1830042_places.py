# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import TERM_REFERENCE
from fluent.migrate.transforms import REPLACE


def migrate(ctx):
    """Bug 1830042 - Convert places.properties to Fluent, part {index}."""

    source = "browser/chrome/browser/places/places.properties"
    target = "browser/browser/places.ftl"
    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("places-locked-prompt"),
                value=REPLACE(
                    source,
                    "lockPrompt.text",
                    {"%1$S": TERM_REFERENCE("brand-short-name")},
                ),
            )
        ],
    )
