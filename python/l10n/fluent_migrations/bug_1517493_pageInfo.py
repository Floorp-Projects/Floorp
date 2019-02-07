# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import VARIABLE_REFERENCE, TERM_REFERENCE
from fluent.migrate import REPLACE, COPY, CONCAT, PLURALS, REPLACE_IN_TEXT

def migrate(ctx):
    """Bug 1517493 - Move strings from pageInfo.dtd and subsection of strings from pageInfo.properties to Fluent, part {index}"""

    ctx.add_transforms(
        "browser/browser/pageInfo.ftl",
        "browser/browser/pageInfo.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("security-site-data-cookies"),
                value=REPLACE(
                    "browser/chrome/browser/pageInfo.properties",
                    "securitySiteDataCookies",
                    {
                        "%1$S": VARIABLE_REFERENCE("value"),
                        "%2$S": VARIABLE_REFERENCE("unit")
                    },
                    normalize_printf=True
                )
            ),
            FTL.Message(
                id=FTL.Identifier("security-site-data-only"),
                value=REPLACE(
                    "browser/chrome/browser/pageInfo.properties",
                    "securitySiteDataOnly",
                    {
                        "%1$S": VARIABLE_REFERENCE("value"),
                        "%2$S": VARIABLE_REFERENCE("unit")
                    },
                    normalize_printf=True
                )
            ),
            FTL.Message(
                id=FTL.Identifier("page-info-window"),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier("style"),
                        CONCAT(
                            FTL.TextElement("width: "),
                            COPY(
                                "browser/chrome/browser/pageInfo.dtd",
                                "pageInfoWindow.width"
                            ),
                            FTL.TextElement("px; min-height: "),
                            COPY(
                                "browser/chrome/browser/pageInfo.dtd",
                                "pageInfoWindow.height"
                            ),
                            FTL.TextElement("px;")
                        )
                    )
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("media-image-type"),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier("value"),
                        REPLACE(
                            "browser/chrome/browser/pageInfo.properties",
                            "mediaImageType",
                            {
                                "%1$S": VARIABLE_REFERENCE("type"),
                            },
                            normalize_printf=True
                        )
                    )
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("media-dimensions-scaled"),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier("value"),
                        REPLACE(
                            "browser/chrome/browser/pageInfo.properties",
                            "mediaDimensionsScaled",
                            {
                                "%1$S": VARIABLE_REFERENCE("dimx"),
                                "%2$S": VARIABLE_REFERENCE("dimy"),
                                "%3$S": VARIABLE_REFERENCE("scaledx"),
                                "%4$S": VARIABLE_REFERENCE("scaledy"),
                            },
                            normalize_printf=True
                        )
                    )
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("media-dimensions"),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier("value"),
                        REPLACE(
                            "browser/chrome/browser/pageInfo.properties",
                            "mediaDimensions",
                            {
                                "%1$S": VARIABLE_REFERENCE("dimx"),
                                "%2$S": VARIABLE_REFERENCE("dimy"),
                            },
                            normalize_printf=True
                        )
                    )
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("media-file-size"),
                value=REPLACE(
                    "browser/chrome/browser/pageInfo.properties",
                    "mediaFileSize",
                    {
                        "%1$S": VARIABLE_REFERENCE("size"),
                    },
                    normalize_printf=True
                )
            ),
            FTL.Message(
                id=FTL.Identifier("media-block-image"),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier("label"),
                        REPLACE(
                            "browser/chrome/browser/pageInfo.properties",
                            "mediaBlockImage",
                            {
                                "%1$S": VARIABLE_REFERENCE("website")
                            },
                            normalize_printf=True
                        )
                    ),
                    FTL.Attribute(
                        FTL.Identifier("accesskey"),
                        COPY(
                            "browser/chrome/browser/pageInfo.dtd",
                            "mediaBlockImage.accesskey"
                        )
                    )
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("page-info-page"),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier("title"),
                        REPLACE(
                            "browser/chrome/browser/pageInfo.properties",
                            "pageInfo.page.title",
                            {
                                "%1$S": VARIABLE_REFERENCE("website"),
                            },
                            normalize_printf=True
                        )
                    )
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("page-info-frame"),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier("title"),
                        REPLACE(
                            "browser/chrome/browser/pageInfo.properties",
                            "pageInfo.frame.title",
                            {
                                "%1$S": VARIABLE_REFERENCE("website"),
                            },
                            normalize_printf=True
                        )
                    )
                ]
            )
        ]
    )

    ctx.add_transforms(
        "browser/browser/pageInfo.ftl",
        "browser/browser/pageInfo.ftl",
        transforms_from(
"""
security-site-data-cookies-only = { COPY("browser/chrome/browser/pageInfo.properties","securitySiteDataCookiesOnly") }
security-site-data-no = { COPY("browser/chrome/browser/pageInfo.properties","securitySiteDataNo") }
image-size-unknown = { COPY("browser/chrome/browser/pageInfo.properties","unknown") }
copy =
    .key = { COPY("browser/chrome/browser/pageInfo.dtd","copy.key") }
menu-copy =
    .label = { COPY("browser/chrome/browser/pageInfo.dtd","copy.label") }
    .accesskey = { COPY("browser/chrome/browser/pageInfo.dtd","copy.accesskey") }
select-all =
    .key = { COPY("browser/chrome/browser/pageInfo.dtd","selectall.key") }
menu-select-all =
    .label = { COPY("browser/chrome/browser/pageInfo.dtd","selectall.label") }
    .accesskey = { COPY("browser/chrome/browser/pageInfo.dtd","selectall.accesskey") }
close-window =
    .key = { COPY("browser/chrome/browser/pageInfo.dtd","selectall.key") }
general-tab =
    .label = { COPY("browser/chrome/browser/pageInfo.dtd","generalTab") }
    .accesskey = { COPY("browser/chrome/browser/pageInfo.dtd","generalTab.accesskey") }
general-title =
    .value = { COPY("browser/chrome/browser/pageInfo.dtd","generalTitle") }
general-url =
    .value = { COPY("browser/chrome/browser/pageInfo.dtd","generalURL") }
general-type =
    .value = { COPY("browser/chrome/browser/pageInfo.dtd","generalType") }
general-mode =
    .value = { COPY("browser/chrome/browser/pageInfo.dtd","generalMode") }
general-size =
    .value = { COPY("browser/chrome/browser/pageInfo.dtd","generalSize") }
general-referrer =
    .value = { COPY("browser/chrome/browser/pageInfo.dtd","generalReferrer") }
general-modified =
    .value = { COPY("browser/chrome/browser/pageInfo.dtd","generalModified") }
general-encoding =
    .value = { COPY("browser/chrome/browser/pageInfo.dtd","generalEncoding2") }
general-meta-name =
    .label = { COPY("browser/chrome/browser/pageInfo.dtd","generalMetaName") }
general-meta-content =
    .label = { COPY("browser/chrome/browser/pageInfo.dtd","generalMetaContent") }
media-tab =
    .label = { COPY("browser/chrome/browser/pageInfo.dtd","mediaTab") }
    .accesskey = { COPY("browser/chrome/browser/pageInfo.dtd","mediaTab.accesskey") }
media-location =
    .value = { COPY("browser/chrome/browser/pageInfo.dtd","mediaLocation") }
media-text =
    .value = { COPY("browser/chrome/browser/pageInfo.dtd","mediaText") }
media-alt-header =
    .label = { COPY("browser/chrome/browser/pageInfo.dtd","mediaAltHeader") }
media-address =
    .label = { COPY("browser/chrome/browser/pageInfo.dtd","mediaAddress") }
media-type =
    .label = { COPY("browser/chrome/browser/pageInfo.dtd","mediaType") }
media-size =
    .label = { COPY("browser/chrome/browser/pageInfo.dtd","mediaSize") }
media-count =
    .label = { COPY("browser/chrome/browser/pageInfo.dtd","mediaCount") }
media-dimension =
    .value = { COPY("browser/chrome/browser/pageInfo.dtd","mediaDimension") }
media-long-desc =
    .value = { COPY("browser/chrome/browser/pageInfo.dtd","mediaLongdesc") }
media-save-as =
    .label = { COPY("browser/chrome/browser/pageInfo.dtd","mediaSaveAs") }
    .accesskey = { COPY("browser/chrome/browser/pageInfo.dtd","mediaSaveAs.accesskey") }
media-save-image-as =
    .label = { COPY("browser/chrome/browser/pageInfo.dtd","mediaSaveAs") }
    .accesskey = { COPY("browser/chrome/browser/pageInfo.dtd","mediaSaveAs2.accesskey") }
media-preview =
    .value = { COPY("browser/chrome/browser/pageInfo.dtd","mediaPreview") }
perm-tab =
    .label = { COPY("browser/chrome/browser/pageInfo.dtd","permTab") }
    .accesskey = { COPY("browser/chrome/browser/pageInfo.dtd","permTab.accesskey") }
permissions-for =
    .value = { COPY("browser/chrome/browser/pageInfo.dtd","permissionsFor") }
security-tab =
    .label = { COPY("browser/chrome/browser/pageInfo.dtd","securityTab") }
    .accesskey = { COPY("browser/chrome/browser/pageInfo.dtd","securityTab.accesskey") }
security-view =
    .label = { COPY("browser/chrome/browser/pageInfo.dtd","securityView.certView") }
    .accesskey = { COPY("browser/chrome/browser/pageInfo.dtd","securityView.accesskey") }
security-view-unknown = { COPY("browser/chrome/browser/pageInfo.dtd","securityView.unknown") }
    .value = { COPY("browser/chrome/browser/pageInfo.dtd","securityView.unknown") }
security-view-identity =
    .value = { COPY("browser/chrome/browser/pageInfo.dtd","securityView.identity.header") }
security-view-identity-owner =
    .value = { COPY("browser/chrome/browser/pageInfo.dtd","securityView.identity.owner") }
security-view-identity-domain =
    .value = { COPY("browser/chrome/browser/pageInfo.dtd","securityView.identity.domain") }
security-view-identity-verifier =
    .value = { COPY("browser/chrome/browser/pageInfo.dtd","securityView.identity.verifier") }
security-view-identity-validity =
    .value = { COPY("browser/chrome/browser/pageInfo.dtd","securityView.identity.validity") }
security-view-privacy =
    .value = { COPY("browser/chrome/browser/pageInfo.dtd","securityView.privacy.header") }
security-view-privacy-history-value = { COPY("browser/chrome/browser/pageInfo.dtd","securityView.privacy.history") }
security-view-privacy-sitedata-value = { COPY("browser/chrome/browser/pageInfo.dtd","securityView.privacy.siteData") }
security-view-privacy-clearsitedata =
    .label = { COPY("browser/chrome/browser/pageInfo.dtd","securityView.privacy.clearSiteData") }
    .accesskey = { COPY("browser/chrome/browser/pageInfo.dtd","securityView.privacy.clearSiteData.accessKey") }
security-view-privacy-passwords-value = { COPY("browser/chrome/browser/pageInfo.dtd","securityView.privacy.passwords") }
security-view-privacy-viewpasswords =
    .label = { COPY("browser/chrome/browser/pageInfo.dtd","securityView.privacy.viewPasswords") }
    .accesskey = { COPY("browser/chrome/browser/pageInfo.dtd","securityView.privacy.viewPasswords.accessKey") }
security-view-technical =
    .value = { COPY("browser/chrome/browser/pageInfo.dtd","securityView.technical.header") }
help-button =
    .label = { COPY("browser/chrome/browser/pageInfo.dtd","helpButton.label") }
unknown = { COPY("browser/chrome/browser/pageInfo.properties","unknown") }
not-set-verified-by = { COPY("browser/chrome/browser/pageInfo.properties","notset") }
not-set-alternative-text = { COPY("browser/chrome/browser/pageInfo.properties","notset") }
not-set-date = { COPY("browser/chrome/browser/pageInfo.properties","notset") }
media-img = { COPY("browser/chrome/browser/pageInfo.properties","mediaImg") }
media-bg-img = { COPY("browser/chrome/browser/pageInfo.properties","mediaBGImg") }
media-border-img = { COPY("browser/chrome/browser/pageInfo.properties","mediaBorderImg") }
media-list-img = { COPY("browser/chrome/browser/pageInfo.properties","mediaListImg") }
media-cursor = { COPY("browser/chrome/browser/pageInfo.properties","mediaCursor") }
media-object = { COPY("browser/chrome/browser/pageInfo.properties","mediaObject") }
media-embed = { COPY("browser/chrome/browser/pageInfo.properties","mediaEmbed") }
media-link = { COPY("browser/chrome/browser/pageInfo.properties","mediaLink") }
media-input = { COPY("browser/chrome/browser/pageInfo.properties","mediaInput") }
media-video = { COPY("browser/chrome/browser/pageInfo.properties","mediaVideo") }
media-audio = { COPY("browser/chrome/browser/pageInfo.properties","mediaAudio") }
saved-passwords-yes = { COPY("browser/chrome/browser/pageInfo.properties","yes") }
saved-passwords-no = { COPY("browser/chrome/browser/pageInfo.properties","no") }
no-page-title =
    .value = { COPY("browser/chrome/browser/pageInfo.properties","noPageTitle") }
general-quirks-mode =
    .value = { COPY("browser/chrome/browser/pageInfo.properties","generalQuirksMode") }
general-strict-mode =
    .value = { COPY("browser/chrome/browser/pageInfo.properties","generalStrictMode") }
security-no-owner = { COPY("browser/chrome/browser/pageInfo.properties","securityNoOwner") }
media-select-folder = { COPY("browser/chrome/browser/pageInfo.properties","mediaSelectFolder") }
media-unknown-not-cached =
    .value = { COPY("browser/chrome/browser/pageInfo.properties","mediaUnknownNotCached") }
permissions-use-default =
    .label = { COPY("browser/chrome/browser/pageInfo.properties","permissions.useDefault") }
security-no-visits = { COPY("browser/chrome/browser/pageInfo.properties","no") }
"""
        )
    )
