# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import MESSAGE_REFERENCE, TERM_REFERENCE, VARIABLE_REFERENCE
from fluent.migrate import COPY, CONCAT, REPLACE


def migrate(ctx):
    """Bug 1523757 - Migrate appMenu notification DTD strings from browser.dtd, part {index}"""
    ctx.add_transforms(
        "browser/browser/appMenuNotifications.ftl",
        "browser/browser/appMenuNotifications.ftl",
        transforms_from(
"""
appmenu-update-whats-new =
    .value = { COPY(from_path,"updateAvailable.whatsnew.label") }
appmenu-addon-private-browsing-installed =
    .buttonlabel = { COPY(from_path,"addonPrivateBrowsing.okButton.label") }
    .buttonaccesskey = { COPY(from_path,"addonPrivateBrowsing.okButton.accesskey") }
appmenu-addon-post-install-incognito-checkbox =
    .label = { COPY(from_path,"addonPostInstall.incognito.checkbox.label") }
    .accesskey = { COPY(from_path,"addonPostInstall.incognito.checkbox.accesskey") }
appmenu-addon-private-browsing =
    .label = { COPY(from_path,"addonPrivateBrowsing.header2.label") }
    .buttonlabel = { COPY(from_path,"addonPrivateBrowsing.manageButton.label") }
    .buttonaccesskey = { COPY(from_path,"addonPrivateBrowsing.manageButton.accesskey") }
    .secondarybuttonlabel = { COPY(from_path,"addonPrivateBrowsing.okButton.label") }
    .secondarybuttonaccesskey = { COPY(from_path,"addonPrivateBrowsing.okButton.accesskey") }
appmenu-addon-private-browsing-learn-more = { COPY(from_path,"addonPrivateBrowsing.learnMore.label") }
""", from_path="browser/chrome/browser/browser.dtd"
        )
    )

    ctx.add_transforms(
        "browser/browser/appMenuNotifications.ftl",
        "browser/browser/appMenuNotifications.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("appmenu-update-available"),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier("label"),
                        REPLACE(
                            "browser/chrome/browser/browser.dtd",
                            "updateAvailable.header.message",
                            {
                                "&brandShorterName;": TERM_REFERENCE("brand-shorter-name"),
                            }
                        )
                    ),
                    FTL.Attribute(
                        FTL.Identifier("buttonlabel"),
                        COPY(
                            "browser/chrome/browser/browser.dtd",
                            "updateAvailable.acceptButton.label",
                        )
                    ),
                    FTL.Attribute(
                        FTL.Identifier("buttonaccesskey"),
                        COPY(
                            "browser/chrome/browser/browser.dtd",
                            "updateAvailable.acceptButton.accesskey",
                        )
                    ),
                    FTL.Attribute(
                        FTL.Identifier("secondarybuttonlabel"),
                        COPY(
                            "browser/chrome/browser/browser.dtd",
                            "updateAvailable.cancelButton.label",
                        )
                    ),
                    FTL.Attribute(
                        FTL.Identifier("secondarybuttonaccesskey"),
                        COPY(
                            "browser/chrome/browser/browser.dtd",
                            "updateAvailable.cancelButton.accesskey",
                        )
                    ),
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("appmenu-update-manual"),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier("label"),
                        REPLACE(
                            "browser/chrome/browser/browser.dtd",
                            "updateManual.header.message",
                            {
                                "&brandShorterName;": TERM_REFERENCE("brand-shorter-name"),
                            }
                        )
                    ),
                    FTL.Attribute(
                        FTL.Identifier("buttonlabel"),
                        REPLACE(
                            "browser/chrome/browser/browser.dtd",
                            "updateManual.acceptButton.label",
                            {
                                "&brandShorterName;": TERM_REFERENCE("brand-shorter-name"),
                            }
                        )
                    ),
                    FTL.Attribute(
                        FTL.Identifier("buttonaccesskey"),
                        COPY(
                            "browser/chrome/browser/browser.dtd",
                            "updateManual.acceptButton.accesskey",
                        )
                    ),
                    FTL.Attribute(
                        FTL.Identifier("secondarybuttonlabel"),
                        COPY(
                            "browser/chrome/browser/browser.dtd",
                            "updateManual.cancelButton.label",
                        )
                    ),
                    FTL.Attribute(
                        FTL.Identifier("secondarybuttonaccesskey"),
                        COPY(
                            "browser/chrome/browser/browser.dtd",
                            "updateManual.cancelButton.accesskey",
                        )
                    ),
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("appmenu-update-restart"),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier("label"),
                        REPLACE(
                            "browser/chrome/browser/browser.dtd",
                            "updateRestart.header.message2",
                            {
                                "&brandShorterName;": TERM_REFERENCE("brand-shorter-name"),
                            }
                        )
                    ),
                    FTL.Attribute(
                        FTL.Identifier("buttonlabel"),
                        COPY(
                            "browser/chrome/browser/browser.dtd",
                            "updateRestart.acceptButton.label",
                        )
                    ),
                    FTL.Attribute(
                        FTL.Identifier("buttonaccesskey"),
                        COPY(
                            "browser/chrome/browser/browser.dtd",
                            "updateRestart.acceptButton.accesskey",
                        )
                    ),
                    FTL.Attribute(
                        FTL.Identifier("secondarybuttonlabel"),
                        COPY(
                            "browser/chrome/browser/browser.dtd",
                            "updateRestart.cancelButton.label",
                        )
                    ),
                    FTL.Attribute(
                        FTL.Identifier("secondarybuttonaccesskey"),
                        COPY(
                            "browser/chrome/browser/browser.dtd",
                            "updateRestart.cancelButton.accesskey",
                        )
                    ),
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("appmenu-update-available-message"),
                value=REPLACE(
                    "browser/chrome/browser/browser.dtd",
                    "updateAvailable.message",
                    {
                        "&brandShorterName;": TERM_REFERENCE("brand-shorter-name")
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier("appmenu-update-manual-message"),
                value=REPLACE(
                    "browser/chrome/browser/browser.dtd",
                    "updateManual.message",
                    {
                        "&brandShorterName;": TERM_REFERENCE("brand-shorter-name")
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier("appmenu-update-restart-message"),
                value=REPLACE(
                    "browser/chrome/browser/browser.dtd",
                    "updateRestart.message2",
                    {
                        "&brandShorterName;": TERM_REFERENCE("brand-shorter-name")
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier("appmenu-addon-post-install-message"),
                value=REPLACE(
                    "browser/chrome/browser/browser.dtd",
                    "addonPostInstallMessage.label",
                    {
                        "<image class='addon-addon-icon'/>": FTL.TextElement("<image data-l10n-name='addon-install-icon'></image>"),
                        "<image class='addon-toolbar-icon'/>": FTL.TextElement("<image data-l10n-name='addon-menu-icon'></image>"),
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier("appmenu-addon-private-browsing-message"),
                value=REPLACE(
                    "browser/chrome/browser/browser.dtd",
                    "addonPrivateBrowsing.body2.label",
                    {
                        "&brandShorterName;": TERM_REFERENCE("brand-shorter-name")
                    }
                )
            ),
        ]
    )
