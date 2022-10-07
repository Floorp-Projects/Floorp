# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import TERM_REFERENCE, transforms_from, VARIABLE_REFERENCE
from fluent.migrate.transforms import (
    COPY,
    LegacySource,
    REPLACE,
    REPLACE_IN_TEXT,
    Transform,
)
import re


class MATCH(LegacySource):
    """Custom transform for extracting parts of a netError.dtd message.

    `start` and `end` are not included in the result, which is always trimmed.
    `index` allows for targeting matches beyond the first (=0) in the source.
    `replacements` are optional; if set, they work as in `REPLACE()`
    """

    def __init__(
        self, path, key, start: str, end: str, index=0, replacements=None, **kwargs
    ):
        super(MATCH, self).__init__(path, key, **kwargs)
        self.start = start
        self.end = end
        self.index = index
        self.replacements = replacements

    def __call__(self, ctx):
        element: FTL.TextElement = super(MATCH, self).__call__(ctx)
        text: str = element.value
        starts = list(re.finditer(re.escape(self.start), text))
        if self.index > len(starts) - 1:
            print(
                f"  WARNING: index {self.index} out of range for {self.start} in {self.key}"
            )
            return Transform.pattern_of(element)
        start = starts[self.index].end()
        end = text.find(self.end, start)
        text = self.trim_text(text[start:end])
        element.value = re.sub("[\n\r]+", " ", text)

        if self.replacements is None:
            return Transform.pattern_of(element)
        else:
            return REPLACE_IN_TEXT(element, self.replacements)(ctx)


def BOLD_VARIABLE_REFERENCE(name):
    return FTL.Pattern(
        [
            FTL.TextElement("<b>"),
            FTL.Placeable(VARIABLE_REFERENCE(name)),
            FTL.TextElement("</b>"),
        ]
    )


# These strings are dropped as unused:
# - cspBlocked.longDesc
# - xfoBlocked.longDesc


def migrate(ctx):
    """Bug 1734217 - Migrate aboutNetError.xhtml from DTD to Fluent, part {index}"""

    source = "browser/chrome/overrides/netError.dtd"
    target = "toolkit/toolkit/neterror/netError.ftl"
    ctx.add_transforms(
        target,
        target,
        transforms_from(
            """

neterror-page-title = { COPY(source, "loadError.label") }
certerror-page-title = { COPY(source, "certerror.pagetitle2") }
certerror-sts-page-title = { COPY(source, "certerror.sts.pagetitle") }
neterror-blocked-by-policy-page-title = { COPY(source, "blockedByPolicy.title") }
neterror-captive-portal-page-title = { COPY(source, "captivePortal.title") }
neterror-malformed-uri-page-title = { COPY(source, "malformedURI.pageTitle") }

neterror-advanced-button = { COPY(source, "advanced2.label") }
neterror-copy-to-clipboard-button = { COPY(source, "certerror.copyToClipboard.label") }
neterror-learn-more-link = { COPY(source, "errorReporting.learnMore") }
neterror-open-portal-login-page-button = { COPY(source, "openPortalLoginPage.label2") }
neterror-override-exception-button = { COPY(source, "securityOverride.exceptionButton1Label") }
neterror-pref-reset-button = { COPY(source, "prefReset.label") }
neterror-return-to-previous-page-button = { COPY(source, "returnToPreviousPage.label") }
neterror-return-to-previous-page-recommended-button = { COPY(source, "returnToPreviousPage1.label") }
neterror-try-again-button = { COPY(source, "retry.label") }
neterror-view-certificate-link = { COPY(source, "viewCertificate.label") }

neterror-pref-reset = { COPY(source, "prefReset.longDesc") }
""",
            source=source,
        )
        + [
            FTL.Message(
                id=FTL.Identifier("neterror-error-reporting-automatic"),
                value=REPLACE(
                    source,
                    "errorReporting.automatic2",
                    replacements={
                        "Mozilla": TERM_REFERENCE("vendor-short-name"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("neterror-generic-error"),
                value=MATCH(
                    source,
                    "generic.longDesc",
                    start="<p>",
                    end="</p>",
                    replacements={
                        "&brandShortName;": TERM_REFERENCE("brand-short-name"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("neterror-load-error-try-again"),
                value=MATCH(
                    source,
                    "sharedLongDesc",
                    start="<li>",
                    end="</li>",
                    index=0,
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("neterror-load-error-connection"),
                value=MATCH(
                    source,
                    "sharedLongDesc",
                    start="<li>",
                    end="</li>",
                    index=1,
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("neterror-load-error-firewall"),
                value=MATCH(
                    source,
                    "sharedLongDesc",
                    start="<li>",
                    end="</li>",
                    index=2,
                    replacements={
                        "&brandShortName;": TERM_REFERENCE("brand-short-name"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("neterror-captive-portal"),
                value=MATCH(
                    source,
                    "captivePortal.longDesc2",
                    start="<p>",
                    end="</p>",
                ),
            ),
        ]
        + transforms_from(
            """
neterror-dns-not-found-title = { COPY_PATTERN(prev, "dns-not-found-title") }
neterror-dns-not-found-with-suggestion = { COPY_PATTERN(prev, "dns-not-found-with-suggestion") }
neterror-dns-not-found-hint-header = { COPY_PATTERN(prev, "dns-not-found-hint-header") }
neterror-dns-not-found-hint-try-again = { COPY_PATTERN(prev, "dns-not-found-hint-try-again") }
neterror-dns-not-found-hint-check-network = { COPY_PATTERN(prev, "dns-not-found-hint-check-network") }
neterror-dns-not-found-hint-firewall = { COPY_PATTERN(prev, "dns-not-found-hint-firewall") }
""",
            prev="browser/browser/netError.ftl",
        )
        + [
            # file-not-found-long-desc = { COPY(source, "fileNotFound.longDesc") }
            # <ul>
            # <li>Check the file name for capitalization or other typing errors.</li>
            # <li>Check to see if the file was moved, renamed or deleted.</li>
            # </ul>
            FTL.Message(
                id=FTL.Identifier("neterror-file-not-found-filename"),
                value=MATCH(
                    source,
                    "fileNotFound.longDesc",
                    start="<li>",
                    end="</li>",
                    index=0,
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("neterror-file-not-found-moved"),
                value=MATCH(
                    source,
                    "fileNotFound.longDesc",
                    start="<li>",
                    end="</li>",
                    index=1,
                ),
            ),
            # file-access-denied-long-desc = { COPY(source, "fileAccessDenied.longDesc") }
            # <ul>
            # <li>It may have been removed, moved, or file permissions may be preventing access.</li>
            # </ul>
            FTL.Message(
                id=FTL.Identifier("neterror-access-denied"),
                value=MATCH(
                    source,
                    "fileAccessDenied.longDesc",
                    start="<li>",
                    end="</li>",
                    index=0,
                ),
            ),
            # unknown-protocol-found-long-desc = { COPY(source, "unknownProtocolFound.longDesc") }
            # <ul>
            # <li>You might need to install other software to open this address.</li>
            # </ul>
            FTL.Message(
                id=FTL.Identifier("neterror-unknown-protocol"),
                value=MATCH(
                    source,
                    "unknownProtocolFound.longDesc",
                    start="<li>",
                    end="</li>",
                    index=0,
                ),
            ),
            # redirect-loop-long-desc = { COPY(source, "redirectLoop.longDesc") }
            # <ul>
            # <li>This problem can sometimes be caused by disabling or refusing to accept
            # cookies.</li>
            # </ul>
            FTL.Message(
                id=FTL.Identifier("neterror-redirect-loop"),
                value=MATCH(
                    source,
                    "redirectLoop.longDesc",
                    start="<li>",
                    end="</li>",
                    index=0,
                ),
            ),
            # unknown-socket-type-long-desc = { COPY(source, "unknownSocketType.longDesc") }
            # <ul>
            # <li>Check to make sure your system has the Personal Security Manager
            # installed.</li>
            # <li>This might be due to a non-standard configuration on the server.</li>
            # </ul>
            FTL.Message(
                id=FTL.Identifier("neterror-unknown-socket-type-psm-installed"),
                value=MATCH(
                    source,
                    "unknownSocketType.longDesc",
                    start="<li>",
                    end="</li>",
                    index=0,
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("neterror-unknown-socket-type-server-config"),
                value=MATCH(
                    source,
                    "unknownSocketType.longDesc",
                    start="<li>",
                    end="</li>",
                    index=1,
                ),
            ),
            # not-cached-long-desc = { COPY(source, "notCached.longDesc") }
            # <p>The requested document is not available in &brandShortName;’s cache.</p>
            # <ul>
            # <li>As a security precaution, &brandShortName; does not automatically re-request sensitive documents.</li>
            # <li>Click Try Again to re-request the document from the website.</li>
            # </ul>
            FTL.Message(
                id=FTL.Identifier("neterror-not-cached-intro"),
                value=MATCH(
                    source,
                    "notCached.longDesc",
                    start="<p>",
                    end="</p>",
                    index=0,
                    replacements={
                        "&brandShortName;": TERM_REFERENCE("brand-short-name"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("neterror-not-cached-sensitive"),
                value=MATCH(
                    source,
                    "notCached.longDesc",
                    start="<li>",
                    end="</li>",
                    index=0,
                    replacements={
                        "&brandShortName;": TERM_REFERENCE("brand-short-name"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("neterror-not-cached-try-again"),
                value=MATCH(
                    source,
                    "notCached.longDesc",
                    start="<li>",
                    end="</li>",
                    index=1,
                ),
            ),
            # net-offline-long-desc = { COPY(source, "netOffline.longDesc2") }
            # <ul>
            # <li>Press "Try Again" to switch to online mode and reload the page.</li>
            # </ul>
            FTL.Message(
                id=FTL.Identifier("neterror-net-offline"),
                value=MATCH(
                    source,
                    "netOffline.longDesc2",
                    start="<li>",
                    end="</li>",
                    index=0,
                    replacements={
                        ' "': FTL.TextElement(" “"),
                        '" ': FTL.TextElement("” "),
                    },
                ),
            ),
            # proxy-resolve-failure-long-desc = { COPY(source, "proxyResolveFailure.longDesc") }
            # <ul>
            # <li>Check the proxy settings to make sure that they are correct.</li>
            # <li>Check to make sure your computer has a working network connection.</li>
            # <li>If your computer or network is protected by a firewall or proxy, make sure
            # that &brandShortName; is permitted to access the Web.</li>
            # </ul>
            FTL.Message(
                id=FTL.Identifier("neterror-proxy-resolve-failure-settings"),
                value=MATCH(
                    source,
                    "proxyResolveFailure.longDesc",
                    start="<li>",
                    end="</li>",
                    index=0,
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("neterror-proxy-resolve-failure-connection"),
                value=MATCH(
                    source,
                    "proxyResolveFailure.longDesc",
                    start="<li>",
                    end="</li>",
                    index=1,
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("neterror-proxy-resolve-failure-firewall"),
                value=MATCH(
                    source,
                    "proxyResolveFailure.longDesc",
                    start="<li>",
                    end="</li>",
                    index=2,
                    replacements={
                        "&brandShortName;": TERM_REFERENCE("brand-short-name"),
                    },
                ),
            ),
            # proxy-connect-failure-long-desc = { COPY(source, "proxyConnectFailure.longDesc") }
            # <ul>
            # <li>Check the proxy settings to make sure that they are correct.</li>
            # <li>Contact your network administrator to make sure the proxy server is
            # working.</li>
            # </ul>
            FTL.Message(
                id=FTL.Identifier("neterror-proxy-connect-failure-settings"),
                value=MATCH(
                    source,
                    "proxyConnectFailure.longDesc",
                    start="<li>",
                    end="</li>",
                    index=0,
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("neterror-proxy-connect-failure-contact-admin"),
                value=MATCH(
                    source,
                    "proxyConnectFailure.longDesc",
                    start="<li>",
                    end="</li>",
                    index=1,
                ),
            ),
            # content-encoding-error-long-desc = { COPY(source, "contentEncodingError.longDesc") }
            # <ul>
            # <li>Please contact the website owners to inform them of this problem.</li>
            # </ul>
            FTL.Message(
                id=FTL.Identifier("neterror-content-encoding-error"),
                value=MATCH(
                    source,
                    "contentEncodingError.longDesc",
                    start="<li>",
                    end="</li>",
                    index=0,
                ),
            ),
            # unsafe-content-type-long-desc = { COPY(source, "unsafeContentType.longDesc") }
            # <ul>
            # <li>Please contact the website owners to inform them of this problem.</li>
            # </ul>
            FTL.Message(
                id=FTL.Identifier("neterror-unsafe-content-type"),
                value=MATCH(
                    source,
                    "unsafeContentType.longDesc",
                    start="<li>",
                    end="</li>",
                    index=0,
                ),
            ),
            # nss-failure2-long-desc = { COPY(source, "nssFailure2.longDesc2") }
            # <ul>
            # <li>The page you are trying to view cannot be shown because the authenticity of the received data could not be verified.</li>
            # <li>Please contact the website owners to inform them of this problem.</li>
            # </ul>
            FTL.Message(
                id=FTL.Identifier("neterror-nss-failure-not-verified"),
                value=MATCH(
                    source,
                    "nssFailure2.longDesc2",
                    start="<li>",
                    end="</li>",
                    index=0,
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("neterror-nss-failure-contact-website"),
                value=MATCH(
                    source,
                    "nssFailure2.longDesc2",
                    start="<li>",
                    end="</li>",
                    index=1,
                ),
            ),
            # certerror-intro-para = { COPY(source, "certerror.introPara2") }
            FTL.Message(
                id=FTL.Identifier("certerror-intro"),
                value=REPLACE(
                    source,
                    "certerror.introPara2",
                    replacements={
                        "&brandShortName;": TERM_REFERENCE("brand-short-name"),
                        "<span class='hostname'/>": BOLD_VARIABLE_REFERENCE("hostname"),
                    },
                ),
            ),
            # certerror-sts-intro-para = { COPY(source, "certerror.sts.introPara") }
            FTL.Message(
                id=FTL.Identifier("certerror-sts-intro"),
                value=REPLACE(
                    source,
                    "certerror.sts.introPara",
                    replacements={
                        "&brandShortName;": TERM_REFERENCE("brand-short-name"),
                        "<span class='hostname'/>": BOLD_VARIABLE_REFERENCE("hostname"),
                    },
                ),
            ),
            # certerror-expired-cert-intro-para = { COPY(source, "certerror.expiredCert.introPara") }
            FTL.Message(
                id=FTL.Identifier("certerror-expired-cert-intro"),
                value=REPLACE(
                    source,
                    "certerror.expiredCert.introPara",
                    replacements={
                        "&brandShortName;": TERM_REFERENCE("brand-short-name"),
                        "<span class='hostname'/>": BOLD_VARIABLE_REFERENCE("hostname"),
                    },
                ),
            ),
            # certerror-mitm-long-desc = { COPY(source, "certerror.mitm.longDesc") }
            FTL.Message(
                id=FTL.Identifier("certerror-mitm"),
                value=REPLACE(
                    source,
                    "certerror.mitm.longDesc",
                    replacements={
                        "<span class='hostname'></span>": BOLD_VARIABLE_REFERENCE(
                            "hostname"
                        ),
                        "<span class='mitm-name'/>": BOLD_VARIABLE_REFERENCE("mitm"),
                    },
                ),
            ),
            # corrupted-content-errorv2-long-desc = { COPY(source, "corruptedContentErrorv2.longDesc") }
            # <p>The page you are trying to view cannot be shown because an error in the data transmission was detected.</p>
            # <ul><li>Please contact the website owners to inform them of this problem.</li></ul>
            FTL.Message(
                id=FTL.Identifier("neterror-corrupted-content-intro"),
                value=MATCH(
                    source,
                    "corruptedContentErrorv2.longDesc",
                    start="<p>",
                    end="</p>",
                    index=0,
                    replacements={
                        "&brandShortName;": TERM_REFERENCE("brand-short-name"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("neterror-corrupted-content-contact-website"),
                value=MATCH(
                    source,
                    "corruptedContentErrorv2.longDesc",
                    start="<li>",
                    end="</li>",
                    index=0,
                    replacements={
                        "&brandShortName;": TERM_REFERENCE("brand-short-name"),
                    },
                ),
            ),
            # neterror-sslv3-used = { COPY(source, "sslv3Used.longDesc2") }
            # Advanced info: SSL_ERROR_UNSUPPORTED_VERSION
            FTL.Message(
                id=FTL.Identifier("neterror-sslv3-used"),
                value=COPY(
                    source,
                    "sslv3Used.longDesc2",
                ),
            ),
            # inadequate-security-error-long-desc = { COPY(source, "inadequateSecurityError.longDesc") }
            # <p><span class='hostname'></span> uses security technology that is outdated and vulnerable to attack. An attacker could easily reveal information which you thought to be safe. The website administrator will need to fix the server first before you can visit the site.</p>
            # <p>Error code: NS_ERROR_NET_INADEQUATE_SECURITY</p>
            FTL.Message(
                id=FTL.Identifier("neterror-inadequate-security-intro"),
                value=MATCH(
                    source,
                    "inadequateSecurityError.longDesc",
                    start="<p>",
                    end="</p>",
                    index=0,
                    replacements={
                        "<span class='hostname'></span>": BOLD_VARIABLE_REFERENCE(
                            "hostname"
                        ),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("neterror-inadequate-security-code"),
                value=MATCH(
                    source,
                    "inadequateSecurityError.longDesc",
                    start="<p>",
                    end="</p>",
                    index=1,
                ),
            ),
            # clock-skew-error-long-desc = { COPY(source, "clockSkewError.longDesc") }
            # Your computer thinks it is <span id='wrongSystemTime_systemDate1'/>, which prevents &brandShortName; from connecting securely. To visit <span class='hostname'></span>, update your computer clock in your system settings to the current date, time, and time zone, and then refresh <span class='hostname'></span>.
            FTL.Message(
                id=FTL.Identifier("neterror-clock-skew-error"),
                value=REPLACE(
                    source,
                    "clockSkewError.longDesc",
                    replacements={
                        "&brandShortName;": TERM_REFERENCE("brand-short-name"),
                        "<span class='hostname'></span>": BOLD_VARIABLE_REFERENCE(
                            "hostname"
                        ),
                        "<span id='wrongSystemTime_systemDate1'/>": FTL.FunctionReference(
                            id=FTL.Identifier("DATETIME"),
                            arguments=FTL.CallArguments(
                                positional=[VARIABLE_REFERENCE("now")],
                                named=[
                                    FTL.NamedArgument(
                                        FTL.Identifier("dateStyle"),
                                        FTL.StringLiteral("medium"),
                                    )
                                ],
                            ),
                        ),
                    },
                ),
            ),
            # network-protocol-error-long-desc = { COPY(source, "networkProtocolError.longDesc") }
            # <p>The page you are trying to view cannot be shown because an error in the network protocol was detected.</p>
            # <ul><li>Please contact the website owners to inform them of this problem.</li></ul>
            FTL.Message(
                id=FTL.Identifier("neterror-network-protocol-error-intro"),
                value=MATCH(
                    source,
                    "networkProtocolError.longDesc",
                    start="<p>",
                    end="</p>",
                    index=0,
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("neterror-network-protocol-error-contact-website"),
                value=MATCH(
                    source,
                    "networkProtocolError.longDesc",
                    start="<li>",
                    end="</li>",
                    index=0,
                ),
            ),
            # certerror-expired-cert-second-para = { COPY(source, "certerror.expiredCert.secondPara2") }
            # It’s likely the website’s certificate is expired, which prevents &brandShortName; from connecting securely. If you visit this site, attackers could try to steal information like your passwords, emails, or credit card details.
            FTL.Message(
                id=FTL.Identifier("certerror-expired-cert-second-para"),
                value=REPLACE(
                    source,
                    "certerror.expiredCert.secondPara2",
                    replacements={
                        "&brandShortName;": TERM_REFERENCE("brand-short-name"),
                    },
                ),
            ),
            # certerror-expired-cert-sts-second-para = { COPY(source, "certerror.expiredCert.sts.secondPara") }
            # It’s likely the website’s certificate is expired, which prevents &brandShortName; from connecting securely.
            FTL.Message(
                id=FTL.Identifier("certerror-expired-cert-sts-second-para"),
                value=REPLACE(
                    source,
                    "certerror.expiredCert.sts.secondPara",
                    replacements={
                        "&brandShortName;": TERM_REFERENCE("brand-short-name"),
                    },
                ),
            ),
            # certerror-what-can-you-do-about-it-title = { COPY(source, "certerror.whatCanYouDoAboutItTitle") }
            # What can you do about it?
            FTL.Message(
                id=FTL.Identifier("certerror-what-can-you-do-about-it-title"),
                value=COPY(
                    source,
                    "certerror.whatCanYouDoAboutItTitle",
                ),
            ),
            # certerror-unknown-issuer-what-can-you-do-about-it = { COPY(source, "certerror.unknownIssuer.whatCanYouDoAboutIt") }
            # <p>The issue is most likely with the website, and there is nothing you can do to resolve it.</p>
            # <p>If you are on a corporate network or using anti-virus software, you can reach out to the support teams for assistance. You can also notify the website’s administrator about the problem.</p>
            FTL.Message(
                id=FTL.Identifier(
                    "certerror-unknown-issuer-what-can-you-do-about-it-website"
                ),
                value=MATCH(
                    source,
                    "certerror.unknownIssuer.whatCanYouDoAboutIt",
                    start="<p>",
                    end="</p>",
                    index=0,
                ),
            ),
            FTL.Message(
                id=FTL.Identifier(
                    "certerror-unknown-issuer-what-can-you-do-about-it-contact-admin"
                ),
                value=MATCH(
                    source,
                    "certerror.unknownIssuer.whatCanYouDoAboutIt",
                    start="<p>",
                    end="</p>",
                    index=1,
                ),
            ),
            # certerror-expired-cert-what-can-you-do-about-it = { COPY(source, "certerror.expiredCert.whatCanYouDoAboutIt2") }
            # <p>Your computer clock is set to <span id='wrongSystemTime_systemDate2'/>. Make sure your computer is set to the correct date, time, and time zone in your system settings, and then refresh <span class='hostname'/>.</p>
            # <p>If your clock is already set to the right time, the website is likely misconfigured, and there is nothing you can do to resolve the issue. You can notify the website’s administrator about the problem.</p>
            FTL.Message(
                id=FTL.Identifier(
                    "certerror-expired-cert-what-can-you-do-about-it-clock"
                ),
                value=MATCH(
                    source,
                    "certerror.expiredCert.whatCanYouDoAboutIt2",
                    start="<p>",
                    end="</p>",
                    index=0,
                    replacements={
                        "<span class='hostname'/>": BOLD_VARIABLE_REFERENCE("hostname"),
                        "<span id='wrongSystemTime_systemDate2'/>": FTL.FunctionReference(
                            id=FTL.Identifier("DATETIME"),
                            arguments=FTL.CallArguments(
                                positional=[VARIABLE_REFERENCE("now")],
                                named=[
                                    FTL.NamedArgument(
                                        FTL.Identifier("dateStyle"),
                                        FTL.StringLiteral("medium"),
                                    )
                                ],
                            ),
                        ),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier(
                    "certerror-expired-cert-what-can-you-do-about-it-contact-website"
                ),
                value=MATCH(
                    source,
                    "certerror.expiredCert.whatCanYouDoAboutIt2",
                    start="<p>",
                    end="</p>",
                    index=1,
                ),
            ),
            # certerror-bad-cert-domain-what-can-you-do-about-it = { COPY(source, "certerror.badCertDomain.whatCanYouDoAboutIt") }
            # <p>The issue is most likely with the website, and there is nothing you can do to resolve it. You can notify the website’s administrator about the problem.</p>
            FTL.Message(
                id=FTL.Identifier("certerror-bad-cert-domain-what-can-you-do-about-it"),
                value=MATCH(
                    source,
                    "certerror.badCertDomain.whatCanYouDoAboutIt",
                    start="<p>",
                    end="</p>",
                    index=0,
                ),
            ),
            # certerror-mitm-what-can-you-do-about-it-antivirus = { COPY(source, "certerror.mitm.whatCanYouDoAboutIt1") }
            # If your antivirus software includes a feature that scans encrypted connections (often called “web scanning” or “https scanning”), you can disable that feature. If that doesn’t work, you can remove and reinstall the antivirus software.
            FTL.Message(
                id=FTL.Identifier("certerror-mitm-what-can-you-do-about-it-antivirus"),
                value=COPY(
                    source,
                    "certerror.mitm.whatCanYouDoAboutIt1",
                ),
            ),
            # certerror-mitm-what-can-you-do-about-it-corporate = { COPY(source, "certerror.mitm.whatCanYouDoAboutIt2") }
            # If you are on a corporate network, you can contact your IT department.
            FTL.Message(
                id=FTL.Identifier("certerror-mitm-what-can-you-do-about-it-corporate"),
                value=COPY(
                    source,
                    "certerror.mitm.whatCanYouDoAboutIt2",
                ),
            ),
            # certerror-mitm-what-can-you-do-about-it-attack = { COPY(source, "certerror.mitm.whatCanYouDoAboutIt3") }
            # If you are not familiar with <span class='mitm-name'/>, then this could be an attack and you should not continue to the site.
            FTL.Message(
                id=FTL.Identifier("certerror-mitm-what-can-you-do-about-it-attack"),
                value=REPLACE(
                    source,
                    "certerror.mitm.whatCanYouDoAboutIt3",
                    replacements={
                        "<span class='mitm-name'/>": BOLD_VARIABLE_REFERENCE("mitm"),
                    },
                ),
            ),
            # certerror-mitm-sts-what-can-you-do-about-it = { COPY(source, "certerror.mitm.sts.whatCanYouDoAboutIt3") }
            # If you are not familiar with <span class='mitm-name'/>, then this could be an attack, and there is nothing you can do to access the site.
            FTL.Message(
                id=FTL.Identifier("certerror-mitm-what-can-you-do-about-it-attack-sts"),
                value=REPLACE(
                    source,
                    "certerror.mitm.sts.whatCanYouDoAboutIt3",
                    replacements={
                        "<span class='mitm-name'/>": BOLD_VARIABLE_REFERENCE("mitm"),
                    },
                ),
            ),
            # certerror-what-should-i-do-bad-sts-cert-explanation = { COPY(source, "certerror.whatShouldIDo.badStsCertExplanation1") }
            # <span class='hostname'></span> has a security policy called HTTP Strict Transport Security (HSTS), which means that &brandShortName; can only connect to it securely. You can’t add an exception to visit this site.
            FTL.Message(
                id=FTL.Identifier(
                    "certerror-what-should-i-do-bad-sts-cert-explanation"
                ),
                value=REPLACE(
                    source,
                    "certerror.whatShouldIDo.badStsCertExplanation1",
                    replacements={
                        "&brandShortName;": TERM_REFERENCE("brand-short-name"),
                        "<span class='hostname'></span>": BOLD_VARIABLE_REFERENCE(
                            "hostname"
                        ),
                    },
                ),
            ),
        ],
    )
