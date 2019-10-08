# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY_PATTERN


def migrate(ctx):
    """Bug 1563026 - Migrate search one-offs xul fragment to use fluent, part {index}."""

    ctx.add_transforms(
        "browser/browser/browser.ftl",
        "browser/browser/browser.ftl",
        transforms_from(
            """
search-one-offs-with-title = { COPY(from_path, "searchWithDesc.label") }

search-one-offs-change-settings-button =
    .label = { COPY(from_path, "changeSearchSettings.button") }
search-one-offs-change-settings-compact-button =
    .tooltiptext = { COPY(from_path, "changeSearchSettings.tooltip") }

search-one-offs-context-open-new-tab =
    .label = { COPY(from_path, "searchInNewTab.label") }
    .accesskey = { COPY(from_path, "searchInNewTab.accesskey") }
search-one-offs-context-set-as-default =
    .label = { COPY(from_path, "searchSetAsDefault.label") }
    .accesskey = { COPY(from_path, "searchSetAsDefault.accesskey") }
""", from_path="browser/chrome/browser/browser.dtd")
    )
