# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from

def migrate(ctx):
    """Bug 1625480 - Migrate remaining notifications strings from browser.dtd to Fluent, part {index}."""

    ctx.add_transforms(
        "browser/browser/appMenuNotifications.ftl",
        "browser/browser/appMenuNotifications.ftl",
    transforms_from(
"""
appmenu-new-tab-controlled =
    .label = { COPY(from_path, "newTabControlled.header.message") }
    .buttonlabel = { COPY(from_path, "newTabControlled.keepButton.label") }
    .buttonaccesskey = { COPY(from_path, "newTabControlled.keepButton.accesskey") }
    .secondarybuttonlabel = { COPY(from_path, "newTabControlled.disableButton.label") }
    .secondarybuttonaccesskey = { COPY(from_path, "newTabControlled.disableButton.accesskey") }
appmenu-homepage-controlled =
    .label = { COPY(from_path, "homepageControlled.header.message") }
    .buttonlabel = { COPY(from_path, "homepageControlled.keepButton.label") }
    .buttonaccesskey = { COPY(from_path, "homepageControlled.keepButton.accesskey") }
    .secondarybuttonlabel = { COPY(from_path, "homepageControlled.disableButton.label") }
    .secondarybuttonaccesskey = { COPY(from_path, "homepageControlled.disableButton.accesskey") }
appmenu-tab-hide-controlled =
    .label = { COPY(from_path, "tabHideControlled.header.message") }
    .buttonlabel = { COPY(from_path, "tabHideControlled.keepButton.label") }
    .buttonaccesskey = { COPY(from_path, "tabHideControlled.keepButton.accesskey") }
    .secondarybuttonlabel = { COPY(from_path, "tabHideControlled.disableButton.label") }
    .secondarybuttonaccesskey = { COPY(from_path, "tabHideControlled.disableButton.accesskey") }
""", from_path="browser/chrome/browser/browser.dtd"))
