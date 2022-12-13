# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

password-quality-meter = Password quality meter

## Change Password dialog

change-device-password-window =
  .title = Change Password

# Variables:
# $tokenName (String) - Security device of the change password dialog
change-password-token = Security Device: { $tokenName }
change-password-old = Current password:
change-password-new = New password:
change-password-reenter = New password (again):

pippki-failed-pw-change = Unable to change password.
pippki-incorrect-pw = You did not enter the correct current password. Please try again.
pippki-pw-change-ok = Password successfully changed.

pippki-pw-empty-warning = Your stored passwords and private keys will not be protected.
pippki-pw-erased-ok = You have deleted your password. { pippki-pw-empty-warning }
pippki-pw-not-wanted = Warning! You have decided not to use a password. { pippki-pw-empty-warning }

pippki-pw-change2empty-in-fips-mode = You are currently in FIPS mode. FIPS requires a non-empty password.

## Reset Primary Password dialog

reset-primary-password-window2 =
  .title = Reset Primary Password
  .style = min-width: 40em
reset-password-button-label =
  .label = Reset
reset-primary-password-text = If you reset your Primary Password, all your stored web and e-mail passwords, personal certificates, and private keys will be forgotten. Are you sure you want to reset your Primary Password?

pippki-reset-password-confirmation-title = Reset Primary Password
pippki-reset-password-confirmation-message = Your Primary Password has been reset.

## Downloading cert dialog

download-cert-window2 =
  .title = Downloading Certificate
  .style = min-width: 46em
download-cert-message = You have been asked to trust a new Certificate Authority (CA).
download-cert-trust-ssl =
  .label = Trust this CA to identify websites.
download-cert-trust-email =
  .label = Trust this CA to identify email users.
download-cert-message-desc = Before trusting this CA for any purpose, you should examine its certificate and its policy and procedures (if available).
download-cert-view-cert =
  .label = View
download-cert-view-text = Examine CA certificate

## Client Authorization Ask dialog

client-auth-window =
  .title = User Identification Request
client-auth-site-description = This site has requested that you identify yourself with a certificate:
client-auth-choose-cert = Choose a certificate to present as identification:
client-auth-cert-details = Details of selected certificate:

## Set password (p12) dialog

set-password-window =
  .title = Choose a Certificate Backup Password
set-password-message = The certificate backup password you set here protects the backup file that you are about to create. You must set this password to proceed with the backup.
set-password-backup-pw =
  .value = Certificate backup password:
set-password-repeat-backup-pw =
  .value = Certificate backup password (again):
set-password-reminder = Important: If you forget your certificate backup password, you will not be able to restore this backup later. Please record it in a safe location.

## Protected authentication alert

# Variables:
# $tokenName (String) - The name of the token to authenticate to (for example, "OS Client Cert Token (Modern)")
protected-auth-alert = Please authenticate to the token “{ $tokenName }”. How to do so depends on the token (for example, using a fingerprint reader or entering a code with a keypad).
