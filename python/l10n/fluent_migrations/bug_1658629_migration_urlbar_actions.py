# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate import REPLACE
from fluent.migrate.helpers import VARIABLE_REFERENCE

def migrate(ctx):
    """Bug 1658629 - Show proper action text when moving through local one-off search buttons, part {index}."""

    ctx.add_transforms(
            "browser/browser/browser.ftl",
            "browser/browser/browser.ftl",
            [
            FTL.Message(
                id=FTL.Identifier("urlbar-result-action-search-w-engine"),
                value=REPLACE(
                    "toolkit/chrome/global/autocomplete.properties",
                    "searchWithEngine",
                    {
                        "%1$S": VARIABLE_REFERENCE("engine")
                    },
                    normalize_printf=True
                )
            )
            ]
    )

    ctx.add_transforms(
            "browser/browser/browser.ftl",
            "browser/browser/browser.ftl",
            [
            FTL.Message(
                id=FTL.Identifier("urlbar-result-action-search-in-private-w-engine"),
                value=REPLACE(
                    "toolkit/chrome/global/autocomplete.properties",
                    "searchInPrivateWindowWithEngine",
                    {
                        "%1$S": VARIABLE_REFERENCE("engine")
                    },
                    normalize_printf=True
                )
            )
            ]
    )

    ctx.add_transforms(
        "browser/browser/browser.ftl",
        "browser/browser/browser.ftl",
        transforms_from(
"""
urlbar-result-action-switch-tab = { COPY(from_path, "switchToTab2") }
urlbar-result-action-search-in-private = { COPY(from_path, "searchInPrivateWindow") }
urlbar-result-action-visit = { COPY(from_path, "visit") }

""", from_path="toolkit/chrome/global/autocomplete.properties"))
