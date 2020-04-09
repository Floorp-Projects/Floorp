# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from

def migrate(ctx):
    """Bug 1609563 - Migrate browser-allTabsMenu.inc.xhtml to Fluent, part {index}."""

    ctx.add_transforms(
        "browser/browser/allTabsMenu.ftl",
        "browser/browser/allTabsMenu.ftl",
    transforms_from(
"""
all-tabs-menu-undo-close-tab =
  .label = { COPY(from_path, "undoCloseTab.label") }
all-tabs-menu-search-tabs =
  .label = { COPY(from_path, "allTabsMenu.searchTabs.label") }
all-tabs-menu-new-user-context =
  .label = { COPY(from_path, "newUserContext.label") }
all-tabs-menu-hidden-tabs =
  .label = { COPY(from_path, "hiddenTabs.label") }
all-tabs-menu-manage-user-context =
  .label = { COPY(from_path, "manageUserContext.label") }
  .accesskey = { COPY(from_path, "manageUserContext.accesskey") }
""", from_path="browser/chrome/browser/browser.dtd"))
