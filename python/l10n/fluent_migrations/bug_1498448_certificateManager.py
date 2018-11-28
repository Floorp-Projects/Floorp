# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY
from fluent.migrate import REPLACE
from fluent.migrate import CONCAT
from fluent.migrate.helpers import VARIABLE_REFERENCE
from fluent.migrate.helpers import TERM_REFERENCE
from fluent.migrate.helpers import MESSAGE_REFERENCE

def migrate(ctx):
    """Bug 1498448 - Migrate Certificate Manager Dialog to use fluent for localization, part {index}."""

    ctx.add_transforms(
        "security/manager/security/certificates/certManager.ftl",
        "security/manager/security/certificates/certManager.ftl",
        transforms_from(
"""
certmgr-title =
    .title = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.title") }

certmgr-tab-mine =
    .label = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.tab.mine") }

certmgr-tab-people =
    .label = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.tab.others2") }

certmgr-tab-servers =
    .label = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.tab.websites3") }

certmgr-tab-ca =
    .label = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.tab.ca") }

certmgr-mine = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.mine2") }
certmgr-people = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.others2") }
certmgr-servers = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.websites3") }
certmgr-ca = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.cas2") }

certmgr-detail-general-tab-title =
    .label = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.detail.general_tab.title") }
    .accesskey = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.detail.general_tab.accesskey") }

certmgr-detail-pretty-print-tab-title =
    .label = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.detail.prettyprint_tab.title") }
    .accesskey = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.detail.prettyprint_tab.accesskey") }

certmgr-pending-label =
    .value = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.pending.label") }

certmgr-subject-info-label =
    .value = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.subjectinfo.label") }

certmgr-issuer-info-label =
    .value = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.issuerinfo.label") }

certmgr-period-of-validity-label =
    .value = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.periodofvalidity.label") }

certmgr-fingerprints-label =
    .value = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.fingerprints.label") }

certmgr-cert-detail =
    .title = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.certdetail.title") }
    .buttonlabelaccept = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.close.label") }
    .buttonaccesskeyaccept = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.close.accesskey") }

certmgr-cert-detail-cn =
    .value = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.certdetail.cn") }

certmgr-cert-detail-o =
    .value = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.certdetail.o") }

certmgr-cert-detail-ou =
    .value = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.certdetail.ou") }

certmgr-cert-detail-serialnumber =
    .value = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.certdetail.serialnumber") }

certmgr-cert-detail-sha256-fingerprint =
    .value = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.certdetail.sha256fingerprint") }

certmgr-cert-detail-sha1-fingerprint =
    .value = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.certdetail.sha1fingerprint") }

certmgr-edit-cert-edit-trust = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.editcert.edittrust") }

certmgr-edit-cert-trust-ssl =
    .label = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.editcert.trustssl") }

certmgr-edit-cert-trust-email =
    .label = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.editcert.trustemail") }

certmgr-cert-name =
    .label = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.certname") }

certmgr-cert-server =
    .label = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.certserver") }

certmgr-override-lifetime
    .label = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.override_lifetime") }

certmgr-token-name =
    .label = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.tokenname") }

certmgr-begins-label =
    .label = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.begins") }

certmgr-expires-label =
    .label = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.expires") }

certmgr-email =
    .label = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.email") }

certmgr-serial =
    .label = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.serial") }

certmgr-view =
    .label = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.view2.label") }
    .accesskey = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.view2.accesskey") }

certmgr-edit =
    .label = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.edit3.label") }
    .accesskey = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.edit3.accesskey") }

certmgr-export =
    .label = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.export.label") }
    .accesskey = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.export.accesskey") }

certmgr-delete =
    .label = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.delete2.label") }
    .accesskey = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.delete2.accesskey") }

certmgr-delete-builtin =
    .label = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.delete_builtin.label") }
    .accesskey = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.delete_builtin.accesskey") }

certmgr-backup =
    .label = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.backup2.label") }
    .accesskey = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.backup2.accesskey") }

certmgr-backup-all =
    .label = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.backupall2.label") }
    .accesskey = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.backupall2.accesskey") }

certmgr-restore =
    .label = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.restore2.label") }
    .accesskey = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.restore2.accesskey") }

certmgr-details =
    .value = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.details.label") }
    .accesskey = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.details.accesskey") }

certmgr-fields=
    .value = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.fields.label") }
    .accesskey = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.fields.accesskey") }

certmgr-hierarchy =
    .value = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.hierarchy.label") }
    .accesskey = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.hierarchy.accesskey2") }

certmgr-add-exception =
    .label = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.addException.label") }
    .accesskey = { COPY("security/manager/chrome/pippki/certManager.dtd", "certmgr.addException.accesskey") }

exception-mgr =
    .title = { COPY("security/manager/chrome/pippki/certManager.dtd", "exceptionMgr.title") }

exception-mgr-extra-button =
    .label = { COPY("security/manager/chrome/pippki/certManager.dtd", "exceptionMgr.exceptionButton.label") }
    .accesskey = { COPY("security/manager/chrome/pippki/certManager.dtd", "exceptionMgr.exceptionButton.accesskey") }

exception-mgr-supplemental-warning = { COPY("security/manager/chrome/pippki/certManager.dtd", "exceptionMgr.supplementalWarning") }

exception-mgr-cert-location-url =
    .value = { COPY("security/manager/chrome/pippki/certManager.dtd", "exceptionMgr.certlocation.url") }

exception-mgr-cert-location-download =
    .label = { COPY("security/manager/chrome/pippki/certManager.dtd", "exceptionMgr.certlocation.download") }
    .accesskey = { COPY("security/manager/chrome/pippki/certManager.dtd", "exceptionMgr.certlocation.accesskey") }

exception-mgr-cert-status-view-cert =
    .label = { COPY("security/manager/chrome/pippki/certManager.dtd", "exceptionMgr.certstatus.viewCert") }
    .accesskey = { COPY("security/manager/chrome/pippki/certManager.dtd", "exceptionMgr.certstatus.accesskey") }

exception-mgr-permanent =
    .label = { COPY("security/manager/chrome/pippki/certManager.dtd", "exceptionMgr.permanent.label") }
    .accesskey = { COPY("security/manager/chrome/pippki/certManager.dtd", "exceptionMgr.permanent.accesskey") }

pk11-bad-password = { COPY("security/manager/chrome/pipnss/pipnss.properties", "PK11BadPassword") }
pkcs12-decode-err = { COPY("security/manager/chrome/pipnss/pipnss.properties", "PKCS12DecodeErr") }
pkcs12-unknown-err-restore = { COPY("security/manager/chrome/pipnss/pipnss.properties", "PKCS12UnknownErrRestore") }
pkcs12-unknown-err-backup = { COPY("security/manager/chrome/pipnss/pipnss.properties", "PKCS12UnknownErrBackup") }
pkcs12-unknown-err = { COPY("security/manager/chrome/pipnss/pipnss.properties", "PKCS12UnknownErr") }
pkcs12-info-no-smartcard-backup = { COPY("security/manager/chrome/pipnss/pipnss.properties", "PKCS12InfoNoSmartcardBackup") }
pkcs12-dup-data = { COPY("security/manager/chrome/pipnss/pipnss.properties", "PKCS12DupData") }

choose-p12-backup-file-dialog = { COPY("security/manager/chrome/pippki/pippki.properties", "chooseP12BackupFileDialog") }
file-browse-pkcs12-spec = { COPY("security/manager/chrome/pippki/pippki.properties", "file_browse_PKCS12_spec") }
choose-p12-restore-file-dialog = { COPY("security/manager/chrome/pippki/pippki.properties", "chooseP12RestoreFileDialog2") }
file-browse-certificate-spec = { COPY("security/manager/chrome/pippki/pippki.properties", "file_browse_Certificate_spec") }
import-ca-certs-prompt = { COPY("security/manager/chrome/pippki/pippki.properties", "importCACertsPrompt") }
import-email-cert-prompt = { COPY("security/manager/chrome/pippki/pippki.properties", "importEmailCertPrompt") }

delete-user-cert-title =
    .title = { COPY("security/manager/chrome/pippki/pippki.properties", "deleteUserCertTitle") }

delete-ssl-cert-title =
    .title = { COPY("security/manager/chrome/pippki/pippki.properties", "deleteSslCertTitle3") }

delete-ca-cert-title =
    .title = { COPY("security/manager/chrome/pippki/pippki.properties", "deleteCaCertTitle2") }

delete-email-cert-title =
    .title = { COPY("security/manager/chrome/pippki/pippki.properties", "deleteEmailCertTitle") }

delete-user-cert-confirm = { COPY("security/manager/chrome/pippki/pippki.properties", "deleteUserCertConfirm") }
delete-user-cert-impact = { COPY("security/manager/chrome/pippki/pippki.properties", "deleteUserCertImpact") }
delete-ssl-cert-confirm = { COPY("security/manager/chrome/pippki/pippki.properties", "deleteSslCertConfirm3") }
delete-ssl-cert-impact = { COPY("security/manager/chrome/pippki/pippki.properties", "deleteSslCertImpact3") }
delete-ca-cert-confirm = { COPY("security/manager/chrome/pippki/pippki.properties", "deleteCaCertConfirm2") }
delete-ca-cert-impact = { COPY("security/manager/chrome/pippki/pippki.properties", "deleteCaCertImpactX2") }
delete-email-cert-confirm = { COPY("security/manager/chrome/pippki/pippki.properties", "deleteEmailCertConfirm") }
delete-email-cert-impact = { COPY("security/manager/chrome/pippki/pippki.properties", "deleteEmailCertImpactDesc") }

cert-verified = { COPY("security/manager/chrome/pippki/pippki.properties", "certVerified") }

not-present =
    .value = { COPY("security/manager/chrome/pippki/pippki.properties", "notPresent") }

verify-ssl-client =
    .value = { COPY("security/manager/chrome/pipnss/pipnss.properties", "VerifySSLClient") }

verify-ssl-server =
    .value = { COPY("security/manager/chrome/pipnss/pipnss.properties", "VerifySSLServer") }

verify-ssl-ca =
    .value = { COPY("security/manager/chrome/pipnss/pipnss.properties", "VerifySSLCA") }

verify-email-signer =
    .value = { COPY("security/manager/chrome/pipnss/pipnss.properties", "VerifyEmailSigner") }

verify-email-recip =
    .value = { COPY("security/manager/chrome/pipnss/pipnss.properties", "VerifyEmailRecip") }

cert-not-verified-cert-revoked = { COPY("security/manager/chrome/pippki/pippki.properties", "certNotVerified_CertRevoked") }
cert-not-verified-cert-expired = { COPY("security/manager/chrome/pippki/pippki.properties", "certNotVerified_CertExpired") }
cert-not-verified-cert-not-trusted = { COPY("security/manager/chrome/pippki/pippki.properties", "certNotVerified_CertNotTrusted") }
cert-not-verified-issuer-not-trusted = { COPY("security/manager/chrome/pippki/pippki.properties", "certNotVerified_IssuerNotTrusted") }
cert-not-verified-issuer-unknown = { COPY("security/manager/chrome/pippki/pippki.properties", "certNotVerified_IssuerUnknown") }
cert-not-verified-ca-invalid = { COPY("security/manager/chrome/pippki/pippki.properties", "certNotVerified_CAInvalid") }
cert-not-verified_algorithm-disabled = { COPY("security/manager/chrome/pippki/pippki.properties", "certNotVerified_AlgorithmDisabled") }
cert-not-verified-unknown = { COPY("security/manager/chrome/pippki/pippki.properties", "certNotVerified_Unknown") }

add-exception-invalid-header = { COPY("security/manager/chrome/pippki/pippki.properties", "addExceptionInvalidHeader") }
add-exception-domain-mismatch-short = { COPY("security/manager/chrome/pippki/pippki.properties", "addExceptionDomainMismatchShort") }
add-exception-domain-mismatch-long = { COPY("security/manager/chrome/pippki/pippki.properties", "addExceptionDomainMismatchLong2") }
add-exception-expired-short = { COPY("security/manager/chrome/pippki/pippki.properties", "addExceptionExpiredShort") }
add-exception-expired-long = { COPY("security/manager/chrome/pippki/pippki.properties", "addExceptionExpiredLong2") }
add-exception-unverified-or-bad-signature-short = { COPY("security/manager/chrome/pippki/pippki.properties", "addExceptionUnverifiedOrBadSignatureShort") }
add-exception-unverified-or-bad-signature-long = { COPY("security/manager/chrome/pippki/pippki.properties", "addExceptionUnverifiedOrBadSignatureLong2") }
add-exception-valid-short = { COPY("security/manager/chrome/pippki/pippki.properties", "addExceptionValidShort") }
add-exception-valid-long = { COPY("security/manager/chrome/pippki/pippki.properties", "addExceptionValidLong") }
add-exception-checking-short = { COPY("security/manager/chrome/pippki/pippki.properties", "addExceptionCheckingShort") }
add-exception-checking-long = { COPY("security/manager/chrome/pippki/pippki.properties", "addExceptionCheckingLong2") }
add-exception-no-cert-short = { COPY("security/manager/chrome/pippki/pippki.properties", "addExceptionNoCertShort") }
add-exception-no-cert-long = { COPY("security/manager/chrome/pippki/pippki.properties", "addExceptionNoCertLong2") }
""")
)

    ctx.add_transforms(
    "security/manager/security/certificates/certManager.ftl",
    "security/manager/security/certificates/certManager.ftl",
    [
        FTL.Message(
            id=FTL.Identifier("edit-trust-ca"),
            value=REPLACE(
                "security/manager/chrome/pippki/pippki.properties",
                "editTrustCA",
                {
                    "%S": VARIABLE_REFERENCE("certName")
                },
            )
        ),

            FTL.Message(
                id=FTL.Identifier("cert-with-serial"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("value"),
                        value=REPLACE(
                            "security/manager/chrome/pippki/pippki.properties",
                            "certWithSerial",
                            {
                                "%1$S": VARIABLE_REFERENCE("serialNumber")
                            },
                        )
                    )
                ]
            ),

            FTL.Message(
                id=FTL.Identifier("cert-viewer-title"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=REPLACE(
                            "security/manager/chrome/pippki/pippki.properties",
                            "certViewerTitle",
                            {
                                "%1$S": VARIABLE_REFERENCE("certName")
                            },
                        )
                    )
                ]
            ),

            FTL.Message(
                id=FTL.Identifier("add-exception-branded-warning"),
                value=REPLACE(
                    "security/manager/chrome/pippki/pippki.properties",
                    "addExceptionBrandedWarning2",
                    {
                        "%S": TERM_REFERENCE("-brand-short-name")
                    },
                )
            ),

            FTL.Message(
                id=FTL.Identifier("certmgr-edit-ca-cert"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(
                                "security/manager/chrome/pippki/certManager.dtd",
                                "certmgr.editcacert.title",
                            )
                        ),
                    FTL.Attribute(
                        id=FTL.Identifier("style"),
                        value=CONCAT(
                            FTL.TextElement("width: 48em;"),
                        )
                    )
                ]
            ),

            FTL.Message(
                id=FTL.Identifier("certmgr-delete-cert"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(
                                "security/manager/chrome/pippki/certManager.dtd",
                                "certmgr.deletecert.title",
                            )
                        ),
                    FTL.Attribute(
                        id=FTL.Identifier("style"),
                        value=CONCAT(
                            FTL.TextElement("width: 48em; height: 24em;"),
                        )
                    )
                ]
            ),

            FTL.Message(
                id=FTL.Identifier("certmgr-begins-value"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("value"),
                        value=FTL.Pattern(
                            elements=[
                                FTL.Placeable(
                                    expression=MESSAGE_REFERENCE("certmgr-begins-label.label")
                                )
                            ]
                        )
                    )
                ]
            ),

            FTL.Message(
                id=FTL.Identifier("certmgr-expires-value"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("value"),
                        value=FTL.Pattern(
                            elements=[
                                FTL.Placeable(
                                    expression=MESSAGE_REFERENCE("certmgr-expires-label.label")
                                )
                            ]
                        )
                    )
                ]
            ),
        ]
    )

