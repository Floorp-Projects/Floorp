# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY, CONCAT, REPLACE
from fluent.migrate.helpers import TERM_REFERENCE


def migrate(ctx):
    """Bug 1568133 - Migrate remaining menubar from dtd to ftl, part {index}"""

    ctx.add_transforms(
        "browser/browser/menubar.ftl",
        "browser/browser/menubar.ftl",
        transforms_from(
            """
menu-application-services =
    .label = { COPY(base_path, "servicesMenuMac.label") }
menu-application-hide-other =
    .label = { COPY(base_path, "hideOtherAppsCmdMac.label") }
menu-application-show-all =
    .label = { COPY(base_path, "showAllAppsCmdMac.label") }
menu-application-touch-bar =
    .label = { COPY(base_path, "touchBarCmdMac.label") }

menu-quit =
    .label =
        { PLATFORM() ->
            [windows] { COPY(browser_path, "quitApplicationCmdWin2.label") }
           *[other] { COPY(browser_path, "quitApplicationCmd.label") }
        }
    .accesskey =
        { PLATFORM() ->
            [windows] { COPY(browser_path, "quitApplicationCmdWin2.accesskey") }
           *[other] { COPY(browser_path, "quitApplicationCmd.accesskey") }
        }

menu-quit-button =
    .label = { menu-quit.label }
""",
            base_path="browser/chrome/browser/baseMenuOverlay.dtd",
            browser_path="browser/chrome/browser/browser.dtd",
        ),
    )

    ctx.add_transforms(
        "browser/browser/menubar.ftl",
        "browser/browser/menubar.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("menu-application-hide-this"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=REPLACE(
                            "browser/chrome/browser/baseMenuOverlay.dtd",
                            "hideThisAppCmdMac2.label",
                            {
                                "&brandShorterName;": TERM_REFERENCE(
                                    "brand-shorter-name"
                                ),
                            },
                        ),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("menu-quit-mac"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=REPLACE(
                            "browser/chrome/browser/browser.dtd",
                            "quitApplicationCmdMac2.label",
                            {
                                "&brandShorterName;": TERM_REFERENCE(
                                    "brand-shorter-name"
                                ),
                            },
                        ),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("menu-quit-button-win"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=FTL.Pattern(
                            elements=[
                                FTL.Placeable(
                                    expression=FTL.MessageReference(
                                        id=FTL.Identifier("menu-quit"),
                                        attribute=FTL.Identifier("label"),
                                    )
                                )
                            ]
                        ),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("tooltip"),
                        value=REPLACE(
                            "browser/chrome/browser/browser.dtd",
                            "quitApplicationCmdWin2.tooltip",
                            {
                                "&brandShorterName;": TERM_REFERENCE(
                                    "brand-shorter-name"
                                ),
                            },
                        ),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("menu-about"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=REPLACE(
                            "browser/chrome/browser/baseMenuOverlay.dtd",
                            "aboutProduct2.label",
                            {
                                "&brandShorterName;": TERM_REFERENCE(
                                    "brand-shorter-name"
                                ),
                            },
                        ),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(
                            "browser/chrome/browser/baseMenuOverlay.dtd",
                            "aboutProduct2.accesskey",
                        ),
                    ),
                ],
            ),
        ],
    )
