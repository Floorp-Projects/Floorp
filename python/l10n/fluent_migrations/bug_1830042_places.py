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
places-search-bookmarks =
    .placeholder = { COPY(source, "searchBookmarks") }
places-search-history =
    .placeholder = { COPY(source, "searchHistory") }
places-search-downloads =
    .placeholder = { COPY(source, "searchDownloads") }
places-view-sortby-name =
  .label = { COPY(source, "view.sortBy.1.name.label") }
  .accesskey = { COPY(source, "view.sortBy.1.name.accesskey") }
places-view-sortby-url =
  .label = { COPY(source, "view.sortBy.1.url.label") }
  .accesskey = { COPY(source, "view.sortBy.1.url.accesskey") }
places-view-sortby-date =
  .label = { COPY(source, "view.sortBy.1.date.label") }
  .accesskey = { COPY(source, "view.sortBy.1.date.accesskey") }
places-view-sortby-visit-count =
  .label = { COPY(source, "view.sortBy.1.visitCount.label") }
  .accesskey = { COPY(source, "view.sortBy.1.visitCount.accesskey") }
places-view-sortby-date-added =
  .label = { COPY(source, "view.sortBy.1.dateAdded.label") }
  .accesskey = { COPY(source, "view.sortBy.1.dateAdded.accesskey") }
places-view-sortby-last-modified =
  .label = { COPY(source, "view.sortBy.1.lastModified.label") }
  .accesskey = { COPY(source, "view.sortBy.1.lastModified.accesskey") }
places-view-sortby-tags =
  .label = { COPY(source, "view.sortBy.1.tags.label") }
  .accesskey = { COPY(source, "view.sortBy.1.tags.accesskey") }
""",
            source=source,
        ),
    )

    ctx.add_transforms(
        "browser/browser/placesPrompts.ftl",
        "browser/browser/placesPrompts.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("places-error-title"),
                value=FTL.Pattern([FTL.Placeable(TERM_REFERENCE("brand-short-name"))]),
            ),
        ]
        + transforms_from(
            """
places-no-title = { COPY(source, "noTitle") }
places-bookmarks-backup-title = { COPY(source, "bookmarksBackupTitle") }
places-bookmarks-restore-alert-title = { COPY(source, "bookmarksRestoreAlertTitle") }
places-bookmarks-restore-alert = { COPY(source, "bookmarksRestoreAlert") }
places-bookmarks-restore-title = { COPY(source, "bookmarksRestoreTitle") }
places-bookmarks-restore-filter-name = { COPY(source, "bookmarksRestoreFilterName") }
places-bookmarks-restore-format-error = { COPY(source, "bookmarksRestoreFormatError") }
places-bookmarks-restore-parse-error = { COPY(source, "bookmarksRestoreParseError") }
places-bookmarks-import = { COPY(source, "SelectImport") }
places-bookmarks-export = { COPY(source, "EnterExport") }
""",
            source=source,
        ),
    )
