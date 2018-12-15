# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY

def migrate(ctx):
    """Bug 1514239 - Migrate cert export messages to use Fluent for localization, part {index}."""

    ctx.add_transforms(
        "security/manager/security/certificates/certManager.ftl",
        "security/manager/security/certificates/certManager.ftl",
        transforms_from(
"""
save-cert-as = { COPY("security/manager/chrome/pippki/pippki.properties", "SaveCertAs") }
cert-format-base64 = { COPY("security/manager/chrome/pippki/pippki.properties", "CertFormatBase64") }
cert-format-base64-chain = { COPY("security/manager/chrome/pippki/pippki.properties", "CertFormatBase64Chain") }
cert-format-der = { COPY("security/manager/chrome/pippki/pippki.properties", "CertFormatDER") }
cert-format-pkcs7 = { COPY("security/manager/chrome/pippki/pippki.properties", "CertFormatPKCS7") }
cert-format-pkcs7-chain = { COPY("security/manager/chrome/pippki/pippki.properties", "CertFormatPKCS7Chain") }
write-file-failure = { COPY("security/manager/chrome/pippki/pippki.properties", "writeFileFailure") }
""")
)

