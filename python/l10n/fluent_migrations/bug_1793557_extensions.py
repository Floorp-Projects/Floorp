# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import TERM_REFERENCE, VARIABLE_REFERENCE
from fluent.migrate.transforms import (
    COPY,
    COPY_PATTERN,
    PLURALS,
    REPLACE,
    REPLACE_IN_TEXT,
)


def migrate(ctx):
    """Bug 1793557 - Convert extension strings to Fluent, part {index}."""

    browser_properties = "browser/chrome/browser/browser.properties"
    browser_ftl = "browser/browser/browser.ftl"
    notifications = "browser/browser/addonNotifications.ftl"
    extensions_ui = "browser/browser/extensionsUI.ftl"
    extensions = "toolkit/toolkit/global/extensions.ftl"
    permissions = "toolkit/toolkit/global/extensionPermissions.ftl"

    ctx.add_transforms(
        browser_ftl,
        browser_ftl,
        [
            FTL.Message(
                id=FTL.Identifier("popup-notification-addon-install-unsigned"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("value"),
                        value=COPY(browser_properties, "addonInstall.unsigned"),
                    )
                ],
            ),
        ],
    )

    ctx.add_transforms(
        notifications,
        notifications,
        [
            FTL.Message(
                id=FTL.Identifier("xpinstall-prompt"),
                value=REPLACE(
                    browser_properties,
                    "xpinstallPromptMessage",
                    {"%1$S": TERM_REFERENCE("brand-short-name")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("xpinstall-prompt-header"),
                value=REPLACE(
                    browser_properties,
                    "xpinstallPromptMessage.header",
                    {"%1$S": VARIABLE_REFERENCE("host")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("xpinstall-prompt-message"),
                value=REPLACE(
                    browser_properties,
                    "xpinstallPromptMessage.message",
                    {"%1$S": VARIABLE_REFERENCE("host")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("xpinstall-prompt-header-unknown"),
                value=COPY(browser_properties, "xpinstallPromptMessage.header.unknown"),
            ),
            FTL.Message(
                id=FTL.Identifier("xpinstall-prompt-message-unknown"),
                value=COPY(
                    browser_properties, "xpinstallPromptMessage.message.unknown"
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("xpinstall-prompt-dont-allow"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(
                            browser_properties, "xpinstallPromptMessage.dontAllow"
                        ),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(
                            browser_properties,
                            "xpinstallPromptMessage.dontAllow.accesskey",
                        ),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("xpinstall-prompt-never-allow"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(
                            browser_properties, "xpinstallPromptMessage.neverAllow"
                        ),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(
                            browser_properties,
                            "xpinstallPromptMessage.neverAllow.accesskey",
                        ),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("xpinstall-prompt-never-allow-and-report"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(
                            browser_properties,
                            "xpinstallPromptMessage.neverAllowAndReport",
                        ),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(
                            browser_properties,
                            "xpinstallPromptMessage.neverAllowAndReport.accesskey",
                        ),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("site-permission-install-first-prompt-midi-header"),
                value=COPY(
                    browser_properties, "sitePermissionInstallFirstPrompt.midi.header"
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("site-permission-install-first-prompt-midi-message"),
                value=COPY(
                    browser_properties, "sitePermissionInstallFirstPrompt.midi.message"
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("xpinstall-prompt-install"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(
                            browser_properties, "xpinstallPromptMessage.install"
                        ),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(
                            browser_properties,
                            "xpinstallPromptMessage.install.accesskey",
                        ),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("xpinstall-disabled-locked"),
                value=COPY(browser_properties, "xpinstallDisabledMessageLocked"),
            ),
            FTL.Message(
                id=FTL.Identifier("xpinstall-disabled"),
                value=COPY(browser_properties, "xpinstallDisabledMessage"),
            ),
            FTL.Message(
                id=FTL.Identifier("xpinstall-disabled-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(browser_properties, "xpinstallDisabledButton"),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(
                            browser_properties, "xpinstallDisabledButton.accesskey"
                        ),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("addon-install-blocked-by-policy"),
                value=REPLACE(
                    browser_properties,
                    "addonInstallBlockedByPolicy",
                    {
                        "%1$S": VARIABLE_REFERENCE("addonName"),
                        "%2$S": VARIABLE_REFERENCE("addonId"),
                        "%3$S": FTL.TextElement(""),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("addon-domain-blocked-by-policy"),
                value=COPY(browser_properties, "addonDomainBlockedByPolicy"),
            ),
            FTL.Message(
                id=FTL.Identifier("addon-install-full-screen-blocked"),
                value=COPY(browser_properties, "addonInstallFullScreenBlocked"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-sideload-menu-item"),
                value=REPLACE(
                    browser_properties,
                    "webextPerms.sideloadMenuItem",
                    {
                        "%1$S": VARIABLE_REFERENCE("addonName"),
                        "%2$S": TERM_REFERENCE("brand-short-name"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-update-menu-item"),
                value=REPLACE(
                    browser_properties,
                    "webextPerms.updateMenuItem",
                    {"%1$S": VARIABLE_REFERENCE("addonName")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("addon-removal-title"),
                value=COPY_PATTERN(browser_ftl, "addon-removal-title"),
            ),
            FTL.Message(
                id=FTL.Identifier("addon-removal-message"),
                value=REPLACE(
                    browser_properties,
                    "webext.remove.confirmation.message",
                    {
                        "%1$S": VARIABLE_REFERENCE("name"),
                        "%2$S": TERM_REFERENCE("brand-shorter-name"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("addon-removal-button"),
                value=COPY(browser_properties, "webext.remove.confirmation.button"),
            ),
            FTL.Message(
                id=FTL.Identifier("addon-removal-abuse-report-checkbox"),
                value=COPY_PATTERN(browser_ftl, "addon-removal-abuse-report-checkbox"),
            ),
            FTL.Message(
                id=FTL.Identifier("addon-downloading-and-verifying"),
                value=PLURALS(
                    browser_properties,
                    "addonDownloadingAndVerifying",
                    VARIABLE_REFERENCE("addonCount"),
                    foreach=lambda n: REPLACE_IN_TEXT(
                        n,
                        {"#1": VARIABLE_REFERENCE("addonCount")},
                    ),
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("addon-download-verifying"),
                value=COPY(browser_properties, "addonDownloadVerifying"),
            ),
            FTL.Message(
                id=FTL.Identifier("addon-install-cancel-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(
                            browser_properties, "addonInstall.cancelButton.label"
                        ),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(
                            browser_properties, "addonInstall.cancelButton.accesskey"
                        ),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("addon-install-accept-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(
                            browser_properties, "addonInstall.acceptButton2.label"
                        ),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(
                            browser_properties, "addonInstall.acceptButton2.accesskey"
                        ),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("addon-confirm-install-message"),
                value=PLURALS(
                    browser_properties,
                    "addonConfirmInstall.message",
                    VARIABLE_REFERENCE("addonCount"),
                    foreach=lambda n: REPLACE_IN_TEXT(
                        n,
                        {
                            "#1": TERM_REFERENCE("brand-short-name"),
                            "#2": VARIABLE_REFERENCE("addonCount"),
                        },
                    ),
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("addon-confirm-install-unsigned-message"),
                value=PLURALS(
                    browser_properties,
                    "addonConfirmInstallUnsigned.message",
                    VARIABLE_REFERENCE("addonCount"),
                    foreach=lambda n: REPLACE_IN_TEXT(
                        n,
                        {
                            "#1": TERM_REFERENCE("brand-short-name"),
                            "#2": VARIABLE_REFERENCE("addonCount"),
                        },
                    ),
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("addon-confirm-install-some-unsigned-message"),
                value=PLURALS(
                    browser_properties,
                    "addonConfirmInstallSomeUnsigned.message",
                    VARIABLE_REFERENCE("addonCount"),
                    foreach=lambda n: REPLACE_IN_TEXT(
                        n,
                        {
                            "#1": TERM_REFERENCE("brand-short-name"),
                            "#2": VARIABLE_REFERENCE("addonCount"),
                        },
                    ),
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("addon-install-error-network-failure"),
                value=COPY(browser_properties, "addonInstallError-1"),
            ),
            FTL.Message(
                id=FTL.Identifier("addon-install-error-incorrect-hash"),
                value=REPLACE(
                    browser_properties,
                    "addonInstallError-2",
                    {"%1$S": TERM_REFERENCE("brand-short-name")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("addon-install-error-corrupt-file"),
                value=COPY(browser_properties, "addonInstallError-3"),
            ),
            FTL.Message(
                id=FTL.Identifier("addon-install-error-file-access"),
                value=REPLACE(
                    browser_properties,
                    "addonInstallError-4",
                    {
                        "%2$S": VARIABLE_REFERENCE("addonName"),
                        "%1$S": TERM_REFERENCE("brand-short-name"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("addon-install-error-not-signed"),
                value=REPLACE(
                    browser_properties,
                    "addonInstallError-5",
                    {"%1$S": TERM_REFERENCE("brand-short-name")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("addon-install-error-invalid-domain"),
                value=REPLACE(
                    browser_properties,
                    "addonInstallError-8",
                    {"%2$S": VARIABLE_REFERENCE("addonName")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("addon-local-install-error-network-failure"),
                value=COPY(browser_properties, "addonLocalInstallError-1"),
            ),
            FTL.Message(
                id=FTL.Identifier("addon-local-install-error-incorrect-hash"),
                value=REPLACE(
                    browser_properties,
                    "addonLocalInstallError-2",
                    {"%1$S": TERM_REFERENCE("brand-short-name")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("addon-local-install-error-corrupt-file"),
                value=COPY(browser_properties, "addonLocalInstallError-3"),
            ),
            FTL.Message(
                id=FTL.Identifier("addon-local-install-error-file-access"),
                value=REPLACE(
                    browser_properties,
                    "addonLocalInstallError-4",
                    {
                        "%2$S": VARIABLE_REFERENCE("addonName"),
                        "%1$S": TERM_REFERENCE("brand-short-name"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("addon-local-install-error-not-signed"),
                value=COPY(browser_properties, "addonLocalInstallError-5"),
            ),
            FTL.Message(
                id=FTL.Identifier("addon-install-error-incompatible"),
                value=REPLACE(
                    browser_properties,
                    "addonInstallErrorIncompatible",
                    {
                        "%3$S": VARIABLE_REFERENCE("addonName"),
                        "%1$S": TERM_REFERENCE("brand-short-name"),
                        "%2$S": VARIABLE_REFERENCE("appVersion"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("addon-install-error-blocklisted"),
                value=REPLACE(
                    browser_properties,
                    "addonInstallErrorBlocklisted",
                    {"%1$S": VARIABLE_REFERENCE("addonName")},
                ),
            ),
        ],
    )

    ctx.add_transforms(
        extensions_ui,
        extensions_ui,
        [
            FTL.Message(
                id=FTL.Identifier("webext-perms-learn-more"),
                value=COPY(browser_properties, "webextPerms.learnMore2"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-default-search-description"),
                value=REPLACE(
                    browser_properties,
                    "webext.defaultSearch.description",
                    {
                        "%1$S": VARIABLE_REFERENCE("addonName"),
                        "%2$S": VARIABLE_REFERENCE("currentEngine"),
                        "%3$S": VARIABLE_REFERENCE("newEngine"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-default-search-yes"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(browser_properties, "webext.defaultSearchYes.label"),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(
                            browser_properties, "webext.defaultSearchYes.accessKey"
                        ),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("webext-default-search-no"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(browser_properties, "webext.defaultSearchNo.label"),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(
                            browser_properties, "webext.defaultSearchNo.accessKey"
                        ),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("addon-post-install-message"),
                value=REPLACE(
                    browser_properties,
                    "addonPostInstall.message3",
                    {"%1$S": VARIABLE_REFERENCE("addonName")},
                ),
            ),
        ],
    )

    ctx.add_transforms(
        extensions,
        extensions,
        [
            FTL.Message(
                id=FTL.Identifier("webext-perms-header"),
                value=REPLACE(
                    browser_properties,
                    "webextPerms.header",
                    {"%1$S": VARIABLE_REFERENCE("extension")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-header-with-perms"),
                value=REPLACE(
                    browser_properties,
                    "webextPerms.headerWithPerms",
                    {"%1$S": VARIABLE_REFERENCE("extension")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-header-unsigned"),
                value=REPLACE(
                    browser_properties,
                    "webextPerms.headerUnsigned",
                    {"%1$S": VARIABLE_REFERENCE("extension")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-header-unsigned-with-perms"),
                value=REPLACE(
                    browser_properties,
                    "webextPerms.headerUnsignedWithPerms",
                    {"%1$S": VARIABLE_REFERENCE("extension")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-add"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(browser_properties, "webextPerms.add.label"),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(browser_properties, "webextPerms.add.accessKey"),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-cancel"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(browser_properties, "webextPerms.cancel.label"),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(browser_properties, "webextPerms.cancel.accessKey"),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-sideload-header"),
                value=REPLACE(
                    browser_properties,
                    "webextPerms.sideloadHeader",
                    {"%1$S": VARIABLE_REFERENCE("extension")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-sideload-text"),
                value=COPY(browser_properties, "webextPerms.sideloadText2"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-sideload-text-no-perms"),
                value=COPY(browser_properties, "webextPerms.sideloadTextNoPerms"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-sideload-enable"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(
                            browser_properties, "webextPerms.sideloadEnable.label"
                        ),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(
                            browser_properties, "webextPerms.sideloadEnable.accessKey"
                        ),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-sideload-cancel"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(
                            browser_properties, "webextPerms.sideloadCancel.label"
                        ),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(
                            browser_properties, "webextPerms.sideloadCancel.accessKey"
                        ),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-update-text"),
                value=REPLACE(
                    browser_properties,
                    "webextPerms.updateText2",
                    {"%1$S": VARIABLE_REFERENCE("extension")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-update-accept"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(
                            browser_properties, "webextPerms.updateAccept.label"
                        ),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(
                            browser_properties, "webextPerms.updateAccept.accessKey"
                        ),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-optional-perms-header"),
                value=REPLACE(
                    browser_properties,
                    "webextPerms.optionalPermsHeader",
                    {"%1$S": VARIABLE_REFERENCE("extension")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-optional-perms-list-intro"),
                value=COPY(browser_properties, "webextPerms.optionalPermsListIntro"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-optional-perms-allow"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(
                            browser_properties, "webextPerms.optionalPermsAllow.label"
                        ),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(
                            browser_properties,
                            "webextPerms.optionalPermsAllow.accessKey",
                        ),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-optional-perms-deny"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(
                            browser_properties, "webextPerms.optionalPermsDeny.label"
                        ),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(
                            browser_properties,
                            "webextPerms.optionalPermsDeny.accessKey",
                        ),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-host-description-all-urls"),
                value=COPY(browser_properties, "webextPerms.hostDescription.allUrls"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-host-description-wildcard"),
                value=REPLACE(
                    browser_properties,
                    "webextPerms.hostDescription.wildcard",
                    {"%1$S": VARIABLE_REFERENCE("domain")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-host-description-too-many-wildcards"),
                value=PLURALS(
                    browser_properties,
                    "webextPerms.hostDescription.tooManyWildcards",
                    VARIABLE_REFERENCE("domainCount"),
                    foreach=lambda n: REPLACE_IN_TEXT(
                        n,
                        {"#1": VARIABLE_REFERENCE("domainCount")},
                    ),
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-host-description-one-site"),
                value=REPLACE(
                    browser_properties,
                    "webextPerms.hostDescription.oneSite",
                    {"%1$S": VARIABLE_REFERENCE("domain")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-host-description-too-many-sites"),
                value=PLURALS(
                    browser_properties,
                    "webextPerms.hostDescription.tooManySites",
                    VARIABLE_REFERENCE("domainCount"),
                    foreach=lambda n: REPLACE_IN_TEXT(
                        n,
                        {"#1": VARIABLE_REFERENCE("domainCount")},
                    ),
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-site-perms-header-with-gated-perms-midi"),
                value=REPLACE(
                    browser_properties,
                    "webextSitePerms.headerWithGatedPerms.midi",
                    {
                        "%1$S": VARIABLE_REFERENCE("hostname"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier(
                    "webext-site-perms-header-with-gated-perms-midi-sysex"
                ),
                value=REPLACE(
                    browser_properties,
                    "webextSitePerms.headerWithGatedPerms.midi-sysex",
                    {
                        "%1$S": VARIABLE_REFERENCE("hostname"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-site-perms-description-gated-perms-midi"),
                value=COPY(
                    browser_properties, "webextSitePerms.descriptionGatedPerms.midi"
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-site-perms-header-with-perms"),
                value=REPLACE(
                    browser_properties,
                    "webextSitePerms.headerWithPerms",
                    {
                        "%1$S": VARIABLE_REFERENCE("extension"),
                        "%2$S": VARIABLE_REFERENCE("hostname"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-site-perms-header-unsigned-with-perms"),
                value=REPLACE(
                    browser_properties,
                    "webextSitePerms.headerUnsignedWithPerms",
                    {
                        "%1$S": VARIABLE_REFERENCE("extension"),
                        "%2$S": VARIABLE_REFERENCE("hostname"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-site-perms-midi"),
                value=COPY(browser_properties, "webextSitePerms.description.midi"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-site-perms-midi-sysex"),
                value=COPY(
                    browser_properties, "webextSitePerms.description.midi-sysex"
                ),
            ),
        ],
    )

    ctx.add_transforms(
        permissions,
        permissions,
        [
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-bookmarks"),
                value=COPY(browser_properties, "webextPerms.description.bookmarks"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-browserSettings"),
                value=COPY(
                    browser_properties, "webextPerms.description.browserSettings"
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-browsingData"),
                value=COPY(browser_properties, "webextPerms.description.browsingData"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-clipboardRead"),
                value=COPY(browser_properties, "webextPerms.description.clipboardRead"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-clipboardWrite"),
                value=COPY(
                    browser_properties, "webextPerms.description.clipboardWrite"
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-declarativeNetRequest"),
                value=COPY(
                    browser_properties, "webextPerms.description.declarativeNetRequest"
                ),
            ),
            FTL.Message(
                id=FTL.Identifier(
                    "webext-perms-description-declarativeNetRequestFeedback"
                ),
                value=COPY(
                    browser_properties,
                    "webextPerms.description.declarativeNetRequestFeedback",
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-devtools"),
                value=COPY(browser_properties, "webextPerms.description.devtools"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-downloads"),
                value=COPY(browser_properties, "webextPerms.description.downloads"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-downloads-open"),
                value=COPY(
                    browser_properties, "webextPerms.description.downloads.open"
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-find"),
                value=COPY(browser_properties, "webextPerms.description.find"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-geolocation"),
                value=COPY(browser_properties, "webextPerms.description.geolocation"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-history"),
                value=COPY(browser_properties, "webextPerms.description.history"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-management"),
                value=COPY(browser_properties, "webextPerms.description.management"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-nativeMessaging"),
                value=REPLACE(
                    browser_properties,
                    "webextPerms.description.nativeMessaging",
                    {"%1$S": TERM_REFERENCE("brand-short-name")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-notifications"),
                value=COPY(browser_properties, "webextPerms.description.notifications"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-pkcs11"),
                value=COPY(browser_properties, "webextPerms.description.pkcs11"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-privacy"),
                value=COPY(browser_properties, "webextPerms.description.privacy"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-proxy"),
                value=COPY(browser_properties, "webextPerms.description.proxy"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-sessions"),
                value=COPY(browser_properties, "webextPerms.description.sessions"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-tabs"),
                value=COPY(browser_properties, "webextPerms.description.tabs"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-tabHide"),
                value=COPY(browser_properties, "webextPerms.description.tabHide"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-topSites"),
                value=COPY(browser_properties, "webextPerms.description.topSites"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-webNavigation"),
                value=COPY(browser_properties, "webextPerms.description.webNavigation"),
            ),
        ],
    )
