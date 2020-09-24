# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate import COPY, PLURALS, REPLACE, REPLACE_IN_TEXT
from fluent.migrate.helpers import TERM_REFERENCE, VARIABLE_REFERENCE, transforms_from


def migrate(ctx):
    """Bug 1634042 - Adding page action labels to fluent, part {index}."""

    ctx.add_transforms(
        "browser/browser/browser.ftl",
        "browser/browser/browser.ftl",
        transforms_from("""
page-action-pin-tab-panel =
    .label = { COPY(from_path, "pinTab.label") }
page-action-pin-tab-urlbar =
    .tooltiptext = { COPY(from_path, "pinTab.label") }
page-action-unpin-tab-panel =
    .label = { COPY(from_path, "unpinTab.label") }
page-action-unpin-tab-urlbar =
    .tooltiptext = { COPY(from_path, "unpinTab.label") }
page-action-copy-url-panel =
    .label = { COPY(from_path, "pageAction.copyLink.label") }
page-action-copy-url-urlbar =
    .tooltiptext = { COPY(from_path, "pageAction.copyLink.label") }
page-action-email-link-panel =
    .label = { COPY(from_path, "emailPageCmd.label") }
page-action-email-link-urlbar =
    .tooltiptext = { COPY(from_path, "emailPageCmd.label") }
page-action-share-url-panel =
    .label = { COPY(from_path, "pageAction.shareUrl.label") }
page-action-share-url-urlbar =
    .tooltiptext = { COPY(from_path, "pageAction.shareUrl.label") }
page-action-share-more-panel =
    .label = { COPY(from_path, "pageAction.shareMore.label") }
page-action-send-tab-not-ready =
    .label = { COPY(from_path, "sendToDevice.syncNotReady.label") }
""", from_path="browser/chrome/browser/browser.dtd"))
    ctx.add_transforms(
        "browser/browser/browser.ftl",
        "browser/browser/browser.ftl",
        [
		FTL.Message(
		    id=FTL.Identifier("page-action-pocket-panel"),
		    attributes=[
		    	FTL.Attribute(
		    		id=FTL.Identifier("label"),
				    value=REPLACE(
				        "browser/chrome/browser/browser.dtd",
				        "saveToPocketCmd.label",
				        {
				            "Pocket": TERM_REFERENCE("pocket-brand-name"),
				        },
				    )
				)
			]
		)
		]
	)
    ctx.add_transforms(
        "browser/browser/browser.ftl",
        "browser/browser/browser.ftl",
        [
		FTL.Message(
		    id=FTL.Identifier("page-action-send-tabs-panel"),
		    attributes=[
		    	FTL.Attribute(
		    		id=FTL.Identifier("label"),
				    value=PLURALS(
				        "browser/chrome/browser/browser.properties",
				        "pageAction.sendTabsToDevice.label",
				        VARIABLE_REFERENCE("tabCount"),
				        lambda text: REPLACE_IN_TEXT(
				            text,
				            {
				                "#1": VARIABLE_REFERENCE("tabCount")
				            }
				        )
				    )
				)
		    ]
		)
		]
	)
    ctx.add_transforms(
        "browser/browser/browser.ftl",
        "browser/browser/browser.ftl",
        [
		FTL.Message(
		    id=FTL.Identifier("page-action-send-tabs-urlbar"),
		    attributes=[
		    	FTL.Attribute(
		    		id=FTL.Identifier("tooltiptext"),
				    value=PLURALS(
				        "browser/chrome/browser/browser.properties",
				        "pageAction.sendTabsToDevice.label",
				        VARIABLE_REFERENCE("tabCount"),
				        lambda text: REPLACE_IN_TEXT(
				            text,
				            {
				                "#1": VARIABLE_REFERENCE("tabCount")
				            }
				        )
				    )
				)
		    ]
		)
		]
	)
