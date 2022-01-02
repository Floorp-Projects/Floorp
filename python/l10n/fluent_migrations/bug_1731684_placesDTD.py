# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from, VARIABLE_REFERENCE
from fluent.migrate import REPLACE, COPY


def migrate(ctx):
    """Bug 1731684 - Migrate places.dtd to Fluent, part {index}"""
    target = "browser/browser/places.ftl"
    reference = "browser/browser/places.ftl"
    ctx.add_transforms(
        target,
        reference,
        transforms_from(
            """
places-library =
  .title = { COPY(from_path, "places.library.title") }
  .style = width:{ COPY(from_path, "places.library.width") }px; height:{ COPY(from_path, "places.library.height") }px;

places-organize-button =
  .label = { COPY(from_path, "organize.label") }
  .tooltiptext = { COPY(from_path, "organize.tooltip") }
  .accesskey = { COPY(from_path, "organize.accesskey") }

places-organize-button-mac =
  .label = { COPY(from_path, "organize.label") }
  .tooltiptext = { COPY(from_path, "organize.tooltip") }

places-file-close =
  .label = { COPY(from_path, "file.close.label") }
  .accesskey = { COPY(from_path, "file.close.accesskey") }

places-cmd-close =
  .key = { COPY(from_path, "cmd.close.key") }

places-view-button =
  .label = { COPY(from_path, "views.label") }
  .tooltiptext = { COPY(from_path, "views.tooltip") }
  .accesskey = { COPY(from_path, "views.accesskey") }

places-view-button-mac =
  .label = { COPY(from_path, "views.label") }
  .tooltiptext = { COPY(from_path, "views.tooltip") }

places-view-menu-columns =
  .label = { COPY(from_path, "view.columns.label") }
  .accesskey = { COPY(from_path, "view.columns.accesskey") }

places-view-menu-sort =
  .label = { COPY(from_path, "view.sort.label") }
  .accesskey = { COPY(from_path, "view.sort.accesskey") }

places-view-sort-unsorted =
  .label = { COPY(from_path, "view.unsorted.label") }
  .accesskey = { COPY(from_path, "view.unsorted.accesskey") }

places-view-sort-ascending =
  .label = { COPY(from_path, "view.sortAscending.label") }
  .accesskey = { COPY(from_path, "view.sortAscending.accesskey") }

places-view-sort-descending =
  .label = { COPY(from_path, "view.sortDescending.label") }
  .accesskey = { COPY(from_path, "view.sortDescending.accesskey") }

places-maintenance-button =
  .label = { COPY(from_path, "maintenance.label") }
  .tooltiptext = { COPY(from_path, "maintenance.tooltip") }
  .accesskey = { COPY(from_path, "maintenance.accesskey") }

places-maintenance-button-mac =
  .label = { COPY(from_path, "maintenance.label") }
  .tooltiptext = { COPY(from_path, "maintenance.tooltip") }

places-cmd-backup =
  .label = { COPY(from_path, "cmd.backup.label") }
  .accesskey = { COPY(from_path, "cmd.backup.accesskey") }

places-cmd-restore =
  .label = { COPY(from_path, "cmd.restore2.label") }
  .accesskey = { COPY(from_path, "cmd.restore2.accesskey") }

places-cmd-restore-from-file =
  .label = { COPY(from_path, "cmd.restoreFromFile.label") }
  .accesskey = { COPY(from_path, "cmd.restoreFromFile.accesskey") }

places-import-bookmarks-from-html =
  .label = { COPY(from_path, "importBookmarksFromHTML.label") }
  .accesskey = { COPY(from_path, "importBookmarksFromHTML.accesskey") }

places-export-bookmarks-to-html =
  .label = { COPY(from_path, "exportBookmarksToHTML.label") }
  .accesskey = { COPY(from_path, "exportBookmarksToHTML.accesskey") }

places-import-other-browser =
  .label = { COPY(from_path, "importOtherBrowser.label") }
  .accesskey = { COPY(from_path, "importOtherBrowser.accesskey") }

places-view-sort-col-name =
  .label = { COPY(from_path, "col.name.label") }

places-view-sort-col-tags =
  .label = { COPY(from_path, "col.tags.label") }

places-view-sort-col-url =
  .label = { COPY(from_path, "col.url.label") }

places-view-sort-col-most-recent-visit =
  .label = { COPY(from_path, "col.mostrecentvisit.label") }

places-view-sort-col-visit-count =
  .label = { COPY(from_path, "col.visitcount.label") }

places-view-sort-col-date-added =
  .label = { COPY(from_path, "col.dateadded.label") }

places-view-sort-col-last-modified =
  .label = { COPY(from_path, "col.lastmodified.label") }

places-cmd-find-key =
  .key = { COPY(from_path, "cmd.find.key") }

places-back-button =
  .tooltiptext = { COPY(from_path, "backButton.tooltip") }

places-forward-button =
  .tooltiptext = { COPY(from_path, "forwardButton.tooltip") }

places-details-pane-select-an-item-description = { COPY(from_path, "detailsPane.selectAnItemText.description") }
""",
            from_path="browser/chrome/browser/places/places.dtd",
        ),
    )
