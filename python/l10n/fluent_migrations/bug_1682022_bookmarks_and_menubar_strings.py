# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1682022 - Convert some bookmarking strings to Fluent, and copy over some menubar strings, part {index}."""
    ctx.add_transforms(
        "browser/browser/browser.ftl",
        "browser/browser/browser.ftl",
        transforms_from(
            """
bookmarks-show-all-bookmarks =
  .label = { COPY(from_path, "showAllBookmarks2.label") }
bookmarks-recent-bookmarks =
  .value = { COPY(from_path, "recentBookmarks.label") }
bookmarks-toolbar-chevron =
  .tooltiptext = { COPY(from_path, "bookmarksToolbarChevron.tooltip") }
bookmarks-sidebar-content =
  .aria-label = { COPY(from_path, "bookmarksButton.label") }
bookmarks-menu-button =
  .label = { COPY(from_path, "bookmarksMenuButton2.label") }
bookmarks-other-bookmarks-menu =
  .label = { COPY(from_path, "bookmarksMenuButton.other.label") }
bookmarks-mobile-bookmarks-menu =
  .label = { COPY(from_path, "bookmarksMenuButton.mobile.label") }
bookmarks-tools-sidebar-visibility =
  .label = { $isVisible ->
     [true] { COPY(from_path, "hideBookmarksSidebar.label") }
    *[other] { COPY(from_path, "viewBookmarksSidebar2.label") }
  }
bookmarks-tools-toolbar-visibility =
  .label = { $isVisible ->
     [true] { COPY(from_path, "hideBookmarksToolbar.label") }
    *[other] { COPY(from_path, "viewBookmarksToolbar.label") }
  }
bookmarks-tools-menu-button-visibility =
  .label = { $isVisible ->
     [true] { COPY(from_path, "removeBookmarksMenu.label") }
    *[other] { COPY(from_path, "addBookmarksMenu.label") }
  }
bookmarks-search =
  .label = { COPY(from_path, "searchBookmarks.label") }
bookmarks-tools =
  .label = { COPY(from_path, "bookmarkingTools.label") }
bookmarks-toolbar =
  .toolbarname = { COPY(from_path, "personalbarCmd.label") }
  .accesskey = { COPY(from_path, "personalbarCmd.accesskey") }
  .aria-label = { COPY(from_path, "personalbar.accessibleLabel") }
bookmarks-toolbar-menu =
  .label = { COPY(from_path, "personalbarCmd.label") }
bookmarks-toolbar-placeholder =
  .title = { COPY(from_path, "bookmarksToolbarItem.label") }
bookmarks-toolbar-placeholder-button =
  .label = { COPY(from_path, "bookmarksToolbarItem.label") }
library-bookmarks-menu =
  .label = { COPY(from_path, "bookmarksSubview.label") }
""",
            from_path="browser/chrome/browser/browser.dtd",
        ),
    )

    ctx.add_transforms(
        "browser/browser/browser.ftl",
        "browser/browser/browser.ftl",
        transforms_from(
            """
library-bookmarks-bookmark-this-page =
  .label = { COPY_PATTERN(from_path, "menu-bookmark-this-page.label") }
library-bookmarks-bookmark-edit =
  .label = { COPY_PATTERN(from_path, "menu-bookmark-edit.label") }

more-menu-go-offline =
  .label = { COPY_PATTERN(from_path, "menu-file-go-offline.label") }
  .accesskey = { COPY_PATTERN(from_path, "menu-file-go-offline.accesskey") }
""",
            from_path="browser/browser/menubar.ftl",
        ),
    )
