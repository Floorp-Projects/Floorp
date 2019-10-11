# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY


def migrate(ctx):
    """Bug 1549049 - Unify Sync Now buttons logic, part {index}."""

    ctx.add_transforms(
        "browser/browser/sync.ftl",
        "browser/browser/sync.ftl",
        transforms_from(
"""
fxa-toolbar-sync-now =
    .label = { COPY(from_path, "syncSyncNowItem.label") }
fxa-toolbar-sync-syncing =
    .label = { COPY(from_path, "syncSyncNowItemSyncing.label") }
fxa-toolbar-sync-syncing-tabs =
    .label = { COPY("services/sync/sync.properties", "syncingtabs.label") }
""", from_path="browser/chrome/browser/browser.dtd"))

    ctx.add_transforms(
        "browser/browser/syncedTabs.ftl",
        "browser/browser/syncedTabs.ftl",
        transforms_from(
"""
synced-tabs-context-sync-now =
    .label = { COPY(from_path, "syncSyncNowItem.label") }
    .accesskey = { COPY(from_path, "syncSyncNowItem.accesskey") }
""", from_path="browser/chrome/browser/browser.dtd"))
