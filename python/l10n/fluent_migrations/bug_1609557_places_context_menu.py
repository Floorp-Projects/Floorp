# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from, TERM_REFERENCE, MESSAGE_REFERENCE
from fluent.migrate import COPY_PATTERN, REPLACE, COPY


def migrate(ctx):
    """Bug 1609557 - Migrate placesContextMenu.inc.xhtml to Fluent, part {index}."""

    ctx.add_transforms(
        'browser/browser/places.ftl',
        'browser/browser/places.ftl',
        transforms_from(
"""
places-open =
    .label = { COPY(from_path, "cmd.open.label") }
    .accesskey = { COPY(from_path, "cmd.open.accesskey") }
places-open-tab =
    .label = { COPY(from_path, "cmd.open_tab.label") }
    .accesskey = { COPY(from_path, "cmd.open_tab.accesskey") }
places-open-all-in-tabs =
    .label = { COPY(from_path, "cmd.open_all_in_tabs.label") }
    .accesskey = { COPY(from_path, "cmd.open_all_in_tabs.accesskey") }
places-open-window =
    .label = { COPY(from_path, "cmd.open_window.label") }
    .accesskey = { COPY(from_path, "cmd.open_window.accesskey") }
places-open-private-window =
    .label = { COPY(from_path, "cmd.open_private_window.label") }
    .accesskey = { COPY(from_path, "cmd.open_private_window.accesskey") }
places-new-bookmark =
    .label = { COPY(from_path, "cmd.new_bookmark.label") }
    .accesskey = { COPY(from_path, "cmd.new_bookmark.accesskey") }
places-new-folder-contextmenu =
    .label = { COPY(from_path, "cmd.new_folder.label") }
    .accesskey = { COPY(from_path, "cmd.context_new_folder.accesskey") }
places-new-folder =
    .label = { COPY(from_path, "cmd.new_folder.label") }
    .accesskey = { COPY(from_path, "cmd.new_folder.accesskey") }
places-new-separator =
    .label = { COPY(from_path, "cmd.new_separator.label") }
    .accesskey = { COPY(from_path, "cmd.new_separator.accesskey") }
places-delete-domain-data =
    .label = { COPY(from_path, "cmd.deleteDomainData.label") }
    .accesskey = { COPY(from_path, "cmd.deleteDomainData.accesskey") }
places-sortby-name =
    .label = { COPY(from_path, "cmd.sortby_name.label") }
    .accesskey = { COPY(from_path, "cmd.context_sortby_name.accesskey") }
places-properties =
    .label = { COPY(from_path, "cmd.properties.label") }
    .accesskey = { COPY(from_path, "cmd.properties.accesskey") }
""", from_path="browser/chrome/browser/places/places.dtd")
    )
