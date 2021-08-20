# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import VARIABLE_REFERENCE
from fluent.migrate import COPY, REPLACE


def migrate(ctx):
    """Bug 1725502 - Migrate strings in main-popupset.inc.xhtml from browser.dtd to browser.ftl, part {index}"""

    ctx.add_transforms(
        "browser/browser/browser.ftl",
        "browser/browser/browser.ftl",
        transforms_from(
            """

ui-tour-info-panel-close =
    .tooltiptext = { COPY(path_dtd, "uiTour.infoPanel.close") }
popups-infobar-dont-show-message =
    .label = { COPY(path_properties, "popupWarningDontShowFromMessage") }
    .accesskey = { COPY(path_dtd, "dontShowMessage.accesskey") }
picture-in-picture-hide-toggle =
    .label = { COPY(path_dtd, "pictureInPictureHideToggle.label") }
    .accesskey = { COPY(path_dtd, "pictureInPictureHideToggle.accesskey") }
""",
            path_dtd="browser/chrome/browser/browser.dtd",
            path_properties="browser/chrome/browser/browser.properties",
        ),
    )

    ctx.add_transforms(
        "browser/browser/browser.ftl",
        "browser/browser/browser.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("popups-infobar-allow"),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier("label"),
                        REPLACE(
                            "browser/chrome/browser/browser.properties",
                            "popupAllow",
                            {"%1$S": VARIABLE_REFERENCE("uriHost")},
                        ),
                    ),
                    FTL.Attribute(
                        FTL.Identifier("accesskey"),
                        COPY(
                            "browser/chrome/browser/browser.dtd",
                            "allowPopups.accesskey",
                        ),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("popups-infobar-block"),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier("label"),
                        REPLACE(
                            "browser/chrome/browser/browser.properties",
                            "popupBlock",
                            {"%1$S": VARIABLE_REFERENCE("uriHost")},
                        ),
                    ),
                    FTL.Attribute(
                        FTL.Identifier("accesskey"),
                        COPY(
                            "browser/chrome/browser/browser.dtd",
                            "allowPopups.accesskey",
                        ),
                    ),
                ],
            ),
        ],
    )
