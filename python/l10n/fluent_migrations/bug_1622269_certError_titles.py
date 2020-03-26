# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from, TERM_REFERENCE
from fluent.migrate import COPY, REPLACE


def migrate(ctx):
    """Bug 1622269 - Migrate cert error titles from netError.dtd to aboutCertError.ftl, part {index}"""
    ctx.add_transforms(
        "browser/browser/aboutCertError.ftl",
        "browser/browser/aboutCertError.ftl",
            transforms_from(
"""
generic-title = { COPY(from_path, "generic.title") }
captivePortal-title = { COPY(from_path, "captivePortal.title") }
dnsNotFound-title = { COPY(from_path, "dnsNotFound.title1") }
fileNotFound-title = { COPY(from_path, "fileNotFound.title") }
fileAccessDenied-title = { COPY(from_path, "fileAccessDenied.title") }
malformedURI-title = { COPY(from_path, "malformedURI.title1") }
unknownProtocolFound-title = { COPY(from_path, "unknownProtocolFound.title") }
connectionFailure-title = { COPY(from_path, "connectionFailure.title") }
netTimeout-title = { COPY(from_path, "netTimeout.title") }
redirectLoop-title = { COPY(from_path, "redirectLoop.title") }
unknownSocketType-title = { COPY(from_path, "unknownSocketType.title") }
netReset-title = { COPY(from_path, "netReset.title") }
notCached-title = { COPY(from_path, "notCached.title") }
netOffline-title = { COPY(from_path, "netOffline.title") }
netInterrupt-title = { COPY(from_path, "netInterrupt.title") }
deniedPortAccess-title = { COPY(from_path, "deniedPortAccess.title") }
proxyResolveFailure-title = { COPY(from_path, "proxyResolveFailure.title") }
proxyConnectFailure-title = { COPY(from_path, "proxyConnectFailure.title") }
contentEncodingError-title = { COPY(from_path, "contentEncodingError.title") }
unsafeContentType-title = { COPY(from_path, "unsafeContentType.title") }
nssFailure2-title = { COPY(from_path, "nssFailure2.title") }
nssBadCert-title = { COPY(from_path, "certerror.longpagetitle2") }
nssBadCert-sts-title = { COPY(from_path, "certerror.sts.longpagetitle") }
cspBlocked-title = { COPY(from_path, "cspBlocked.title") }
xfoBlocked-title = { COPY(from_path, "xfoBlocked.title") }
remoteXUL-title = { COPY(from_path, "remoteXUL.title") }
corruptedContentError-title = { COPY(from_path, "corruptedContentErrorv2.title") }
sslv3Used-title = { COPY(from_path, "sslv3Used.title") }
inadequateSecurityError-title = { COPY(from_path, "inadequateSecurityError.title") }
blockedByPolicy-title = { COPY(from_path, "blockedByPolicy.title") }
clockSkewError-title = { COPY(from_path, "clockSkewError.title") }
networkProtocolError-title = { COPY(from_path, "networkProtocolError.title") }
""", from_path="browser/chrome/overrides/netError.dtd"))
    ctx.add_transforms(
        "browser/browser/aboutCertError.ftl",
                        "browser/browser/aboutCertError.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("certerror-mitm-title"),
                value=REPLACE(
                    "browser/chrome/overrides/netError.dtd",
                    "certerror.mitm.title",
                    {
                        "&brandShortName;": TERM_REFERENCE("brand-short-name"),
                    },
                ),
            ),
        ]
    )
