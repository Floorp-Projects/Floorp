# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from, TERM_REFERENCE, VARIABLE_REFERENCE
from fluent.migrate import COPY, REPLACE


def migrate(ctx):
    """Bug 1635553 - Migrate navigator-toolbox to Fluent, part {index}."""
    ctx.add_transforms(
        "browser/browser/browser.ftl",
        "browser/browser/browser.ftl",
        transforms_from(
            """
navbar-accessible =
    .aria-label = { COPY(from_path, "navbar.accessibleLabel") }

navbar-downloads =
    .label = { COPY(from_path, "downloads.label") }

navbar-overflow =
    .tooltiptext = { COPY(from_path, "navbarOverflow.label") }

navbar-print-tab-modal-disabled =
    .label = { COPY(from_path, "printButton.label") }
    .tooltiptext = { COPY(from_path, "printButton.tooltip") }

navbar-library =
    .label = { COPY("browser/chrome/browser/places/places.dtd", "places.library.title") }
    .tooltiptext = { COPY(from_path, "libraryButton.tooltip") }

navbar-search =
    .title = { COPY(from_path, "searchItem.title") }

navbar-accessibility-indicator =
    .tooltiptext = { COPY(from_path, "accessibilityIndicator.tooltip") }

tabs-toolbar =
    .aria-label = { COPY(from_path, "tabsToolbar.label") }

tabs-toolbar-new-tab =
    .label = { COPY(from_path, "tabCmd.label") }

tabs-toolbar-list-all-tabs =
    .label = { COPY(from_path, "listAllTabs.label") }
    .tooltiptext = { COPY(from_path, "listAllTabs.label") }
""",
            from_path="browser/chrome/browser/browser.dtd",
        ),
    )

    ctx.add_transforms(
        "browser/browser/browser.ftl",
        "browser/browser/browser.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("navbar-print"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(
                            "browser/chrome/browser/browser.dtd", "printButton.label"
                        ),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=REPLACE(
                            "browser/chrome/browser/browser.properties",
                            "printButton.tooltip",
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
                id=FTL.Identifier("navbar-home"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(
                            "browser/chrome/browser/browser.dtd", "homeButton.label"
                        ),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=REPLACE(
                            "browser/chrome/browser/browser.dtd",
                            "homeButton.defaultPage.tooltip",
                            {
                                "&brandShortName;": TERM_REFERENCE("brand-short-name"),
                            },
                        ),
                    ),
                ],
            ),
        ],
    )

    ctx.add_transforms(
        "browser/browser/toolbarContextMenu.ftl",
        "browser/browser/toolbarContextMenu.ftl",
        transforms_from(
            """
toolbar-context-menu-menu-bar-cmd =
    .toolbarname = { COPY(from_path, "menubarCmd.label") }
    .accesskey = { COPY(from_path, "menubarCmd.accesskey") }
""",
            from_path="browser/chrome/browser/browser.dtd",
        ),
    )
