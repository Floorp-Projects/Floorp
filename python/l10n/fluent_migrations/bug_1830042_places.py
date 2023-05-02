# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import TERM_REFERENCE, transforms_from, VARIABLE_REFERENCE
from fluent.migrate.transforms import COPY, PLURALS, REPLACE, REPLACE_IN_TEXT


def migrate(ctx):
    """Bug 1830042 - Convert places.properties to Fluent, part {index}."""

    source = "browser/chrome/browser/places/places.properties"
    target = "browser/browser/places.ftl"
    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("places-details-pane-no-items"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("value"),
                        value=COPY(source, "detailsPane.noItems"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("places-details-pane-items-count"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("value"),
                        value=PLURALS(
                            source,
                            "detailsPane.itemsCountLabel",
                            VARIABLE_REFERENCE("count"),
                            foreach=lambda n: REPLACE_IN_TEXT(
                                n,
                                {"#1": VARIABLE_REFERENCE("count")},
                            ),
                        ),
                    )
                ],
            ),
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
places-empty-bookmarks-folder =
  .label = { COPY(source, "bookmarksMenuEmptyFolder") }
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
