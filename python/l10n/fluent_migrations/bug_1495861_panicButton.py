# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY, REPLACE

def migrate(ctx):
    """Bug 1517508 - Migrate panicButton to use Fluent for localization, part {index}."""

    ctx.add_transforms(
        "browser/browser/panicButton.ftl",
        "browser/browser/panicButton.ftl",
        transforms_from(
"""
panic-button-open-new-window = { COPY("browser/chrome/browser/browser.dtd", "panicButton.view.openNewWindow") }
panic-button-undo-warning = { COPY("browser/chrome/browser/browser.dtd", "panicButton.view.undoWarning") }
panic-button-forget-button =
    .label = { COPY("browser/chrome/browser/browser.dtd", "panicButton.view.forgetButton") }
panic-main-timeframe-desc = { COPY("browser/chrome/browser/browser.dtd", "panicButton.view.mainTimeframeDesc") }
panic-button-5min =
    .label = { COPY("browser/chrome/browser/browser.dtd", "panicButton.view.5min") }
panic-button-2hr =
    .label = { COPY("browser/chrome/browser/browser.dtd", "panicButton.view.2hr") }
panic-button-day =
    .label = { COPY("browser/chrome/browser/browser.dtd", "panicButton.view.day") }
panic-button-action-desc = { COPY("browser/chrome/browser/browser.dtd", "panicButton.view.mainActionDesc") }
""")
)

    ctx.add_transforms(
        "browser/browser/panicButton.ftl",
        "browser/browser/panicButton.ftl", [
		FTL.Message(
			id=FTL.Identifier("panic-button-delete-cookies"),
			value=REPLACE(
				"browser/chrome/browser/browser.dtd",
				"panicButton.view.deleteCookies",
				{
					"html:strong>": FTL.TextElement('strong>')
				}
			)
		),
		FTL.Message(
			id=FTL.Identifier("panic-button-delete-history"),
			value=REPLACE(
				"browser/chrome/browser/browser.dtd",
				"panicButton.view.deleteHistory",
				{
					"html:strong>": FTL.TextElement('strong>')
				}
			)
		),
		FTL.Message(
			id=FTL.Identifier("panic-button-delete-tabs-and-windows"),
			value=REPLACE(
				"browser/chrome/browser/browser.dtd",
				"panicButton.view.deleteTabsAndWindows",
				{
					"html:strong>": FTL.TextElement('strong>')
				}
			)
		)
])
