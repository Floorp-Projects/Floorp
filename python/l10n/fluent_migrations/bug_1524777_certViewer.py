# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY

def migrate(ctx):
    """Bug 1524777 - Convert the certificate viewer's XUL grid to HTML table, part {index}."""

    ctx.add_transforms(
        "security/manager/security/certificates/certManager.ftl",
        "security/manager/security/certificates/certManager.ftl",
        transforms_from(
"""
certmgr-subject-label = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.subjectinfo.label") }

certmgr-issuer-label = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.issuerinfo.label") }

certmgr-period-of-validity = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.periodofvalidity.label") }

certmgr-fingerprints = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.fingerprints.label") }

certmgr-cert-detail-commonname = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.certdetail.cn") }

certmgr-cert-detail-org = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.certdetail.o") }

certmgr-cert-detail-orgunit = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.certdetail.ou") }

certmgr-cert-detail-serial-number = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.certdetail.serialnumber") }

certmgr-cert-detail-sha-256-fingerprint = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.certdetail.sha256fingerprint") }

certmgr-cert-detail-sha-1-fingerprint = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.certdetail.sha1fingerprint") }

certmgr-begins-on = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.begins") }

certmgr-expires-on = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.expires") }

""")
)
