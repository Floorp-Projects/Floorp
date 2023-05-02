# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import TERM_REFERENCE, transforms_from
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
            ),
        ]
        + transforms_from(
            """
places-delete-page =
    .label =
        { $count ->
            [1] { COPY(source, "cmd.deleteSinglePage.label") }
           *[other] { COPY(source, "cmd.deleteMultiplePages.label") }
        }
    .accesskey = { COPY(source, "cmd.deleteSinglePage.accesskey") }
places-create-bookmark =
    .label =
        { $count ->
            [1] { COPY(source, "cmd.bookmarkSinglePage2.label") }
           *[other] { COPY(source, "cmd.bookmarkMultiplePages2.label") }
        }
    .accesskey = { COPY(source, "cmd.bookmarkSinglePage2.accesskey") }
""",
            source=source,
        ),
    )
