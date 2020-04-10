# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from

def migrate(ctx):
    """Bug 1624713 - Convert editBookmarkOverlay to Fluent, part {index}."""

    ctx.add_transforms(
        "browser/browser/editBookmarkOverlay.ftl",
        "browser/browser/editBookmarkOverlay.ftl",
    transforms_from(
"""
bookmark-overlay-name =
    .value = { COPY(from_path, "editBookmarkOverlay.name.label") }
    .accesskey = { COPY(from_path, "editBookmarkOverlay.name.accesskey") }
bookmark-overlay-location =
    .value = { COPY(from_path, "editBookmarkOverlay.location.label") }
    .accesskey = { COPY(from_path, "editBookmarkOverlay.location.accesskey") }
bookmark-overlay-folder =
    .value = { COPY(from_path, "editBookmarkOverlay.folder.label") }
bookmark-overlay-choose =
    .label = { COPY(from_path, "editBookmarkOverlay.choose.label") }
bookmark-overlay-folders-expander =
  .tooltiptext = { COPY(from_path, "editBookmarkOverlay.foldersExpanderDown.tooltip") }
  .tooltiptextdown = { bookmark-overlay-folders-expander.tooltiptext }
  .tooltiptextup = { COPY(from_path, "editBookmarkOverlay.expanderUp.tooltip") }
bookmark-overlay-new-folder-button =
    .label = { COPY(from_path, "editBookmarkOverlay.newFolderButton.label") }
    .accesskey = { COPY(from_path, "editBookmarkOverlay.newFolderButton.accesskey") }
bookmark-overlay-tags =
    .value = { COPY(from_path, "editBookmarkOverlay.tags.label") }
    .accesskey ={ COPY(from_path, "editBookmarkOverlay.tags.accesskey") }
bookmark-overlay-tags-empty-description =
    .placeholder = { COPY(from_path, "editBookmarkOverlay.tagsEmptyDesc.label") }
bookmark-overlay-tags-expander =
  .tooltiptext = { COPY(from_path, "editBookmarkOverlay.tagsExpanderDown.tooltip") }
  .tooltiptextdown = { bookmark-overlay-tags-expander.tooltiptext }
  .tooltiptextup = { COPY(from_path, "editBookmarkOverlay.expanderUp.tooltip") }
bookmark-overlay-keyword =
    .value = { COPY(from_path, "editBookmarkOverlay.keyword.label") }
    .accesskey = { COPY(from_path, "editBookmarkOverlay.keyword.accesskey") }
""", from_path="browser/chrome/browser/places/editBookmarkOverlay.dtd"))
