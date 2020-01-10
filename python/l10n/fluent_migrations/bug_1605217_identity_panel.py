# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import COPY, TERM_REFERENCE
from fluent.migrate import REPLACE, CONCAT

def migrate(ctx):
    """Bug 1605217 - Migrate Identity Panel to Fluent., part {index}"""

    ctx.add_transforms(
        "browser/browser/browser.ftl",
        "browser/browser/browser.ftl",
        transforms_from(
            """

identity-connection-not-secure = { COPY(path1, "identity.connectionNotSecure2") }

identity-connection-secure = { COPY(path1, "identity.connectionSecure3") }

identity-connection-file = { COPY(path1, "identity.connectionFile") }

identity-extension-page = { COPY(path1, "identity.extensionPage") }

identity-custom-root = { COPY(path1, "identity.customRoot") }

identity-passive-loaded = { COPY(path1, "identity.passiveLoaded") }

identity-active-loaded = { COPY(path1, "identity.activeLoaded") }

identity-weak-encryption = { COPY(path1, "identity.weakEncryption") }

identity-insecure-login-forms = { COPY(path1, "identity.insecureLoginForms2") }

identity-permissions =
    .value = { COPY(path1, "identity.permissions3") }
identity-permissions-reload-hint = { COPY(path1, "identity.permissionsReloadHint") }

identity-permissions-empty = { COPY(path1, "identity.permissionsEmpty") }

identity-clear-site-data =
    .label = { COPY(path1, "identity.clearSiteData") }
identity-connection-not-secure-security-view = { COPY(path1, "identity.connectionNotSecureSecurityView") }

identity-connection-verified = { COPY(path1, "identity.connectionVerified3") }

identity-ev-owner-label = { COPY(path1, "identity.evOwnerLabel2") }

identity-remove-cert-exception =
    .label = { COPY(path1, "identity.removeCertException.label") }
    .accesskey = { COPY(path1, "identity.removeCertException.accesskey") }
identity-description-insecure = { COPY(path1, "identity.description.insecure") }

identity-description-insecure-login-forms = { COPY(path1, "identity.description.insecureLoginForms") }

identity-description-weak-cipher-intro = { COPY(path1, "identity.description.weakCipher") }

identity-description-weak-cipher-risk = { COPY(path1, "identity.description.weakCipher2") }

identity-description-passive-loaded = { COPY(path1, "identity.description.passiveLoaded") }

identity-description-passive-loaded-insecure = { COPY(path1, "identity.description.passiveLoaded2") } <label data-l10n-name="link">{ COPY(path1, "identity.learnMore") }</label>

identity-description-active-loaded = { COPY(path1, "identity.description.activeLoaded") }

identity-description-active-loaded-insecure = { COPY(path1, "identity.description.activeLoaded2") }

identity-learn-more =
    .value = { COPY(path1, "identity.learnMore") }

identity-disable-mixed-content-blocking =
    .label = { COPY(path1, "identity.disableMixedContentBlocking.label") }
    .accesskey = { COPY(path1, "identity.disableMixedContentBlocking.accesskey") }
identity-enable-mixed-content-blocking =
    .label = { COPY(path1, "identity.enableMixedContentBlocking.label") }
    .accesskey = { COPY(path1, "identity.enableMixedContentBlocking.accesskey") }
identity-more-info-link-text =
    .label = { COPY(path1, "identity.moreInfoLinkText2") }
""", path1="browser/chrome/browser/browser.dtd"))

    ctx.add_transforms(
        "browser/browser/browser.ftl",
        "browser/browser/browser.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("identity-connection-internal"),
                value=REPLACE(
                    "browser/chrome/browser/browser.dtd",
                    "identity.connectionInternal",
                    {
                        "&brandShortName;": TERM_REFERENCE("brand-short-name")
                    },
                )
            ),
            FTL.Message(
                id=FTL.Identifier("identity-active-blocked"),
                value=REPLACE(
                    "browser/chrome/browser/browser.dtd",
                    "identity.activeBlocked",
                    {
                        "&brandShortName;": TERM_REFERENCE("brand-short-name")
                    },
                )
            ),
            FTL.Message(
                id=FTL.Identifier("identity-description-active-blocked"),
                value=CONCAT(
                    REPLACE(
                        "browser/chrome/browser/browser.dtd",
                        "identity.description.activeBlocked",
                        {
                            "&brandShortName;": TERM_REFERENCE("brand-short-name")
                        },
                    ),
                    FTL.TextElement(' <label data-l10n-name="link">'),
                    COPY(
                        "browser/chrome/browser/browser.dtd",
                        "identity.learnMore",
                    ),
                    FTL.TextElement('</label>'),
                )
            ),
            FTL.Message(
                id=FTL.Identifier("identity-description-custom-root"),
                value=CONCAT(
                    REPLACE(
                        "browser/chrome/browser/browser.dtd",
                        "identity.description.customRoot",
                        {
                            "&brandShortName;": TERM_REFERENCE("brand-short-name")
                        },
                    ),
                    FTL.TextElement(' <label data-l10n-name="link">'),
                    COPY(
                        "browser/chrome/browser/browser.dtd",
                        "identity.learnMore",
                    ),
                    FTL.TextElement('</label>'),
                )
            ),
            FTL.Message(
                id=FTL.Identifier("identity-description-passive-loaded-mixed"),
                value=CONCAT(
                    REPLACE(
                        "browser/chrome/browser/browser.dtd",
                        "identity.description.passiveLoaded3",
                        {
                            "&brandShortName;": TERM_REFERENCE("brand-short-name")
                        },
                    ),
                    FTL.TextElement(' <label data-l10n-name="link">'),
                    COPY(
                        "browser/chrome/browser/browser.dtd",
                        "identity.learnMore",
                    ),
                    FTL.TextElement('</label>'),
                )
            ),
        ]
    )
