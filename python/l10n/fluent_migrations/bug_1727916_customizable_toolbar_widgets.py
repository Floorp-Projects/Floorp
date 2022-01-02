# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from, VARIABLE_REFERENCE
from fluent.migrate import COPY, COPY_PATTERN, REPLACE


def migrate(ctx):
    """Bug 1727916 - Switch several customizable toolbar item names to sentence case, part {index}"""

    ctx.add_transforms(
        "browser/browser/browser.ftl",
        "browser/browser/browser.ftl",
        transforms_from(
            """
toolbar-button-email-link =
    .label = { COPY(from_path, "email-link-button.label") }
    .tooltiptext = { COPY(from_path, "email-link-button.tooltiptext3") }
toolbar-button-synced-tabs =
    .label = { COPY(from_path, "remotetabs-panelmenu.label") }
    .tooltiptext = { COPY(from_path, "remotetabs-panelmenu.tooltiptext2") }
""",
            from_path="browser/chrome/browser/customizableui/customizableWidgets.properties",
        ),
    )

    ctx.add_transforms(
        "browser/browser/browser.ftl",
        "browser/browser/browser.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("toolbar-button-save-page"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(
                            "browser/chrome/browser/customizableui/customizableWidgets.properties",
                            "save-page-button.label",
                        ),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=REPLACE(
                            "browser/chrome/browser/customizableui/customizableWidgets.properties",
                            "save-page-button.tooltiptext3",
                            {"%1$S": VARIABLE_REFERENCE("shortcut")},
                        ),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("toolbar-button-open-file"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(
                            "browser/chrome/browser/customizableui/customizableWidgets.properties",
                            "open-file-button.label",
                        ),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=REPLACE(
                            "browser/chrome/browser/customizableui/customizableWidgets.properties",
                            "open-file-button.tooltiptext3",
                            {"%1$S": VARIABLE_REFERENCE("shortcut")},
                        ),
                    ),
                ],
            ),
        ],
    )

    ctx.add_transforms(
        "browser/browser/browser.ftl",
        "browser/browser/browser.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("toolbar-button-new-private-window"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY_PATTERN(
                            "browser/browser/appmenu.ftl",
                            "appmenuitem-new-private-window.label",
                        ),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=REPLACE(
                            "browser/chrome/browser/customizableui/customizableWidgets.properties",
                            "privatebrowsing-button.tooltiptext",
                            {"%1$S": VARIABLE_REFERENCE("shortcut")},
                        ),
                    ),
                ],
            ),
        ],
    )
