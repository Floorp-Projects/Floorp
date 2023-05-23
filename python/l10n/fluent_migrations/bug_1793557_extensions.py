# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import TERM_REFERENCE, VARIABLE_REFERENCE
from fluent.migrate.transforms import COPY, PLURALS, REPLACE, REPLACE_IN_TEXT


def migrate(ctx):
    """Bug 1793557 - Convert Extension.jsm to Fluent, part {index}."""

    source = "browser/chrome/browser/browser.properties"
    extensions = "toolkit/toolkit/global/extensions.ftl"
    permissions = "toolkit/toolkit/global/extensionPermissions.ftl"

    ctx.add_transforms(
        extensions,
        extensions,
        [
            FTL.Message(
                id=FTL.Identifier("webext-perms-header"),
                value=REPLACE(
                    source,
                    "webextPerms.header",
                    {"%1$S": VARIABLE_REFERENCE("extension")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-header-with-perms"),
                value=REPLACE(
                    source,
                    "webextPerms.headerWithPerms",
                    {"%1$S": VARIABLE_REFERENCE("extension")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-header-unsigned"),
                value=REPLACE(
                    source,
                    "webextPerms.headerUnsigned",
                    {"%1$S": VARIABLE_REFERENCE("extension")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-header-unsigned-with-perms"),
                value=REPLACE(
                    source,
                    "webextPerms.headerUnsignedWithPerms",
                    {"%1$S": VARIABLE_REFERENCE("extension")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-add"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(source, "webextPerms.add.label"),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(source, "webextPerms.add.accessKey"),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-cancel"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(source, "webextPerms.cancel.label"),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(source, "webextPerms.cancel.accessKey"),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-sideload-header"),
                value=REPLACE(
                    source,
                    "webextPerms.sideloadHeader",
                    {"%1$S": VARIABLE_REFERENCE("extension")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-sideload-text"),
                value=COPY(source, "webextPerms.sideloadText2"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-sideload-text-no-perms"),
                value=COPY(source, "webextPerms.sideloadTextNoPerms"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-sideload-enable"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(source, "webextPerms.sideloadEnable.label"),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(source, "webextPerms.sideloadEnable.accessKey"),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-sideload-cancel"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(source, "webextPerms.sideloadCancel.label"),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(source, "webextPerms.sideloadCancel.accessKey"),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-update-text"),
                value=REPLACE(
                    source,
                    "webextPerms.updateText2",
                    {"%1$S": VARIABLE_REFERENCE("extension")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-update-accept"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(source, "webextPerms.updateAccept.label"),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(source, "webextPerms.updateAccept.accessKey"),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-optional-perms-header"),
                value=REPLACE(
                    source,
                    "webextPerms.optionalPermsHeader",
                    {"%1$S": VARIABLE_REFERENCE("extension")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-optional-perms-list-intro"),
                value=COPY(source, "webextPerms.optionalPermsListIntro"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-optional-perms-allow"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(source, "webextPerms.optionalPermsAllow.label"),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(source, "webextPerms.optionalPermsAllow.accessKey"),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-optional-perms-deny"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(source, "webextPerms.optionalPermsDeny.label"),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(source, "webextPerms.optionalPermsDeny.accessKey"),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-host-description-all-urls"),
                value=COPY(source, "webextPerms.hostDescription.allUrls"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-host-description-wildcard"),
                value=REPLACE(
                    source,
                    "webextPerms.hostDescription.wildcard",
                    {"%1$S": VARIABLE_REFERENCE("domain")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-host-description-too-many-wildcards"),
                value=PLURALS(
                    source,
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
                    source,
                    "webextPerms.hostDescription.oneSite",
                    {"%1$S": VARIABLE_REFERENCE("domain")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-host-description-too-many-sites"),
                value=PLURALS(
                    source,
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
                    source,
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
                    source,
                    "webextSitePerms.headerWithGatedPerms.midi-sysex",
                    {
                        "%1$S": VARIABLE_REFERENCE("hostname"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-site-perms-description-gated-perms-midi"),
                value=COPY(source, "webextSitePerms.descriptionGatedPerms.midi"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-site-perms-header-with-perms"),
                value=REPLACE(
                    source,
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
                    source,
                    "webextSitePerms.headerUnsignedWithPerms",
                    {
                        "%1$S": VARIABLE_REFERENCE("extension"),
                        "%2$S": VARIABLE_REFERENCE("hostname"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-site-perms-midi"),
                value=COPY(source, "webextSitePerms.description.midi"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-site-perms-midi-sysex"),
                value=COPY(source, "webextSitePerms.description.midi-sysex"),
            ),
        ],
    )

    ctx.add_transforms(
        permissions,
        permissions,
        [
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-bookmarks"),
                value=COPY(source, "webextPerms.description.bookmarks"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-browserSettings"),
                value=COPY(source, "webextPerms.description.browserSettings"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-browsingData"),
                value=COPY(source, "webextPerms.description.browsingData"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-clipboardRead"),
                value=COPY(source, "webextPerms.description.clipboardRead"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-clipboardWrite"),
                value=COPY(source, "webextPerms.description.clipboardWrite"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-declarativeNetRequest"),
                value=COPY(source, "webextPerms.description.declarativeNetRequest"),
            ),
            FTL.Message(
                id=FTL.Identifier(
                    "webext-perms-description-declarativeNetRequestFeedback"
                ),
                value=COPY(
                    source, "webextPerms.description.declarativeNetRequestFeedback"
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-devtools"),
                value=COPY(source, "webextPerms.description.devtools"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-downloads"),
                value=COPY(source, "webextPerms.description.downloads"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-downloads-open"),
                value=COPY(source, "webextPerms.description.downloads.open"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-find"),
                value=COPY(source, "webextPerms.description.find"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-geolocation"),
                value=COPY(source, "webextPerms.description.geolocation"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-history"),
                value=COPY(source, "webextPerms.description.history"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-management"),
                value=COPY(source, "webextPerms.description.management"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-nativeMessaging"),
                value=REPLACE(
                    source,
                    "webextPerms.description.nativeMessaging",
                    {"%1$S": TERM_REFERENCE("brand-short-name")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-notifications"),
                value=COPY(source, "webextPerms.description.notifications"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-pkcs11"),
                value=COPY(source, "webextPerms.description.pkcs11"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-privacy"),
                value=COPY(source, "webextPerms.description.privacy"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-proxy"),
                value=COPY(source, "webextPerms.description.proxy"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-sessions"),
                value=COPY(source, "webextPerms.description.sessions"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-tabs"),
                value=COPY(source, "webextPerms.description.tabs"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-tabHide"),
                value=COPY(source, "webextPerms.description.tabHide"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-topSites"),
                value=COPY(source, "webextPerms.description.topSites"),
            ),
            FTL.Message(
                id=FTL.Identifier("webext-perms-description-webNavigation"),
                value=COPY(source, "webextPerms.description.webNavigation"),
            ),
        ],
    )
