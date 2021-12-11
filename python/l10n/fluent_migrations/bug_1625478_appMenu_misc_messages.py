# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY, COPY_PATTERN


def migrate(ctx):
    """Bug 1625478 - Convert appMenu* strings from browser.dtd to appmenu.ftl, part {index}."""
    ctx.add_transforms(
        "browser/browser/appmenu.ftl",
        "browser/browser/appmenu.ftl",
        transforms_from(
            """
appmenuitem-help =
    .label = { COPY(from_path, "appMenuHelp.label") }
appmenu-remote-tabs-showall =
    .label = { COPY(from_path, "appMenuRemoteTabs.showAll.label") }
    .tooltiptext = { COPY(from_path, "appMenuRemoteTabs.showAll.tooltip") }
appmenu-remote-tabs-showmore =
    .label = { COPY_PATTERN("browser/browser/appmenu.ftl", "appmenu-fxa-show-more-tabs")}
    .tooltiptext = { COPY(from_path, "appMenuRemoteTabs.showMore.tooltip") }
appmenu-remote-tabs-notabs = { COPY(from_path, "appMenuRemoteTabs.notabs.label") }
appmenu-remote-tabs-tabsnotsyncing = { COPY(from_path, "appMenuRemoteTabs.tabsnotsyncing.label") }
appmenu-remote-tabs-noclients = { COPY(from_path, "appMenuRemoteTabs.noclients.subtitle") }
appmenu-remote-tabs-connectdevice =
  .label = { COPY(from_path, "appMenuRemoteTabs.connectdevice.label") }
appmenu-remote-tabs-welcome = { COPY(from_path, "appMenuRemoteTabs.welcome.label") }
appmenu-remote-tabs-unverified = { COPY(from_path, "appMenuRemoteTabs.unverified.label") }
""",
            from_path="browser/chrome/browser/browser.dtd",
        ),
    )
