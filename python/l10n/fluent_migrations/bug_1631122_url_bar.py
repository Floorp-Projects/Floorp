# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate import REPLACE
from fluent.migrate.helpers import COPY, TERM_REFERENCE, transforms_from

def migrate(ctx):
    """Bug 1631122 - Convert URL bar dtd strings in browser.xhtml to Fluent, part {index}."""

    ctx.add_transforms(
        "browser/browser/browser.ftl",
        "browser/browser/browser.ftl",
    transforms_from(
"""
urlbar-default-placeholder =
    .defaultPlaceholder = { COPY(from_path, "urlbar.placeholder2") }
urlbar-placeholder =
    .placeholder = { COPY(from_path, "urlbar.placeholder2") }
urlbar-remote-control-notification-anchor =
    .tooltiptext = { COPY(from_path, "urlbar.remoteControlNotificationAnchor.tooltip") }
urlbar-permissions-granted =
    .tooltiptext = { COPY(from_path, "urlbar.permissionsGranted.tooltip") }
urlbar-switch-to-tab =
    .value = { COPY(from_path, "urlbar.switchToTab.label") }
urlbar-extension =
    .value = { COPY(from_path, "urlbar.extension.label") }
urlbar-go-end-cap =
    .tooltiptext = { COPY(from_path, "goEndCap.tooltip") }
urlbar-page-action-button =
    .tooltiptext = { COPY(from_path, "pageActionButton.tooltip") }
""", from_path="browser/chrome/browser/browser.dtd"))

    ctx.add_transforms(
        "browser/browser/browser.ftl",
        "browser/browser/browser.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("urlbar-pocket-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=REPLACE(
                            "browser/chrome/browser/browser.dtd",
                            "pocketButton.tooltiptext",
                            {
                                "Pocket": TERM_REFERENCE("pocket-brand-name"),
                            }
                        )
                    ),
                ]
            ),
        ]
    )
