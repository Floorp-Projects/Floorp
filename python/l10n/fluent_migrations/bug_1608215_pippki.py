# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from

def migrate(ctx):
    """Bug 1608215 - Migrate pippki to Fluent, part {index}."""

    ctx.add_transforms(
        "security/manager/security/pippki/pippki.ftl",
        "security/manager/security/pippki/pippki.ftl",
    transforms_from(
"""
password-quality-meter = { COPY(from_path, "setPassword.meter.label") }
change-password-window =
    .title = { COPY(from_path, "setPassword.title") }
change-password-token = { COPY(from_path, "setPassword.tokenName.label") }: { $tokenName }
change-password-old = { COPY(from_path, "setPassword.oldPassword.label") }
change-password-new = { COPY(from_path, "setPassword.newPassword.label") }
change-password-reenter = { COPY(from_path, "setPassword.reenterPassword.label") }
reset-password-window =
    .title = { COPY(from_path, "resetPassword.title") }
    .style = width: 40em
reset-password-button-label =
  .label = { COPY(from_path, "resetPasswordButtonLabel") }
reset-password-text = { COPY(from_path, "resetPassword.text") }
download-cert-window =
    .title = { COPY(from_path, "downloadCert.title") }
    .style = width: 46em
download-cert-message = { COPY(from_path, "downloadCert.message1") }
download-cert-trust-ssl =
    .label = { COPY(from_path, "downloadCert.trustSSL") }
download-cert-trust-email =
    .label = { COPY(from_path, "downloadCert.trustEmail") }
download-cert-message-desc = { COPY(from_path, "downloadCert.message3") }
download-cert-view-cert =
    .label = { COPY(from_path, "downloadCert.viewCert.label") }
download-cert-view-text = { COPY(from_path, "downloadCert.viewCert.text") }
client-auth-window =
    .title = { COPY(from_path, "clientAuthAsk.title") }
client-auth-site-description = { COPY(from_path, "clientAuthAsk.message1") }
client-auth-choose-cert = { COPY(from_path, "clientAuthAsk.message2") }
client-auth-cert-details = { COPY(from_path, "clientAuthAsk.message3") }
set-password-window =
    .title = { COPY(from_path, "pkcs12.setpassword.title") }
set-password-message = { COPY(from_path, "pkcs12.setpassword.message") }
set-password-backup-pw =
    .value = { COPY(from_path, "pkcs12.setpassword.label1") }
set-password-repeat-backup-pw =
    .value = { COPY(from_path, "pkcs12.setpassword.label2") }
set-password-reminder = { COPY(from_path, "pkcs12.setpassword.reminder") }
protected-auth-window =
    .title = { COPY(from_path, "protectedAuth.title") }
protected-auth-msg = { COPY(from_path, "protectedAuth.msg") }
protected-auth-token = { COPY(from_path, "protectedAuth.tokenName.label") }
""", from_path="security/manager/chrome/pippki/pippki.dtd"))
