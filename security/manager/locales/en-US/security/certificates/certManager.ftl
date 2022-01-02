# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

certmgr-title =
    .title = Certificate Manager

certmgr-tab-mine =
    .label = Your Certificates

certmgr-tab-remembered =
    .label = Authentication Decisions

certmgr-tab-people =
    .label = People

certmgr-tab-servers =
    .label = Servers

certmgr-tab-ca =
    .label = Authorities

certmgr-mine = You have certificates from these organizations that identify you
certmgr-remembered = These certificates are used to identify you to websites
certmgr-people = You have certificates on file that identify these people
certmgr-server = These entries identify server certificate error exceptions
certmgr-ca = You have certificates on file that identify these certificate authorities

certmgr-edit-ca-cert =
    .title = Edit CA certificate trust settings
    .style = width: 48em;

certmgr-edit-cert-edit-trust = Edit trust settings:

certmgr-edit-cert-trust-ssl =
    .label = This certificate can identify websites.

certmgr-edit-cert-trust-email =
    .label = This certificate can identify mail users.

certmgr-delete-cert =
    .title = Delete Certificate
    .style = width: 48em; height: 24em;

certmgr-cert-host =
    .label = Host

certmgr-cert-name =
    .label = Certificate Name

certmgr-cert-server =
    .label = Server

certmgr-override-lifetime =
    .label = Lifetime

certmgr-token-name =
    .label = Security Device

certmgr-begins-label =
    .label = Begins On

certmgr-expires-label =
    .label = Expires On

certmgr-email =
    .label = E-Mail Address

certmgr-serial =
    .label = Serial Number

certmgr-view =
    .label = View…
    .accesskey = V

certmgr-edit =
    .label = Edit Trust…
    .accesskey = E

certmgr-export =
    .label = Export…
    .accesskey = x

certmgr-delete =
    .label = Delete…
    .accesskey = D

certmgr-delete-builtin =
    .label = Delete or Distrust…
    .accesskey = D

certmgr-backup =
    .label = Backup…
    .accesskey = B

certmgr-backup-all =
    .label = Backup All…
    .accesskey = k

certmgr-restore =
    .label = Import…
    .accesskey = m

certmgr-add-exception =
    .label = Add Exception…
    .accesskey = x

exception-mgr =
    .title = Add Security Exception

exception-mgr-extra-button =
    .label = Confirm Security Exception
    .accesskey = C

exception-mgr-supplemental-warning = Legitimate banks, stores, and other public sites will not ask you to do this.

exception-mgr-cert-location-url =
    .value = Location:

exception-mgr-cert-location-download =
    .label = Get Certificate
    .accesskey = G

exception-mgr-cert-status-view-cert =
    .label = View…
    .accesskey = V

exception-mgr-permanent =
    .label = Permanently store this exception
    .accesskey = P

pk11-bad-password = The password entered was incorrect.
pkcs12-decode-err = Failed to decode the file. Either it is not in PKCS #12 format, has been corrupted, or the password you entered was incorrect.
pkcs12-unknown-err-restore = Failed to restore the PKCS #12 file for unknown reasons.
pkcs12-unknown-err-backup = Failed to create the PKCS #12 backup file for unknown reasons.
pkcs12-unknown-err = The PKCS #12 operation failed for unknown reasons.
pkcs12-info-no-smartcard-backup = It is not possible to back up certificates from a hardware security device such as a smart card.
pkcs12-dup-data = The certificate and private key already exist on the security device.

## PKCS#12 file dialogs

choose-p12-backup-file-dialog = File Name to Backup
file-browse-pkcs12-spec = PKCS12 Files
choose-p12-restore-file-dialog = Certificate File to Import

## Import certificate(s) file dialog

file-browse-certificate-spec = Certificate Files
import-ca-certs-prompt = Select File containing CA certificate(s) to import
import-email-cert-prompt = Select File containing somebody’s Email certificate to import

## For editing certificates trust

# Variables:
#   $certName: the name of certificate
edit-trust-ca = The certificate “{ $certName }” represents a Certificate Authority.

## For Deleting Certificates

delete-user-cert-title =
    .title = Delete your Certificates
delete-user-cert-confirm = Are you sure you want to delete these certificates?
delete-user-cert-impact = If you delete one of your own certificates, you can no longer use it to identify yourself.


delete-ssl-override-title =
    .title = Delete Server Certificate Exception
delete-ssl-override-confirm = Are you sure you want to delete this server exception?
delete-ssl-override-impact = If you delete a server exception, you restore the usual security checks for that server and require it uses a valid certificate.

delete-ca-cert-title =
    .title = Delete or Distrust CA Certificates
delete-ca-cert-confirm = You have requested to delete these CA certificates. For built-in certificates all trust will be removed, which has the same effect. Are you sure you want to delete or distrust?
delete-ca-cert-impact = If you delete or distrust a certificate authority (CA) certificate, this application will no longer trust any certificates issued by that CA.


delete-email-cert-title =
    .title = Delete E-Mail Certificates
delete-email-cert-confirm = Are you sure you want to delete these people’s e-mail certificates?
delete-email-cert-impact = If you delete a person’s e-mail certificate, you will no longer be able to send encrypted e-mail to that person.

# Used for semi-uniquely representing a cert.
#
# Variables:
#   $serialNumber : the serial number of the cert in AA:BB:CC hex format.
cert-with-serial =
    .value = Certificate with serial number: { $serialNumber }

# Used to indicate that the user chose not to send a client authentication certificate to a server that requested one in a TLS handshake.
send-no-client-certificate = Send no client certificate

# Used when no cert is stored for an override
no-cert-stored-for-override = (Not Stored)

# When a certificate is unavailable (for example, it has been deleted or the token it exists on has been removed).
certificate-not-available = (Unavailable)

## Used to show whether an override is temporary or permanent

permanent-override = Permanent
temporary-override = Temporary

## Add Security Exception dialog

add-exception-branded-warning = You are about to override how { -brand-short-name } identifies this site.
add-exception-invalid-header = This site attempts to identify itself with invalid information.
add-exception-domain-mismatch-short = Wrong Site
add-exception-domain-mismatch-long = The certificate belongs to a different site, which could mean that someone is trying to impersonate this site.
add-exception-expired-short = Outdated Information
add-exception-expired-long = The certificate is not currently valid. It may have been stolen or lost, and could be used by someone to impersonate this site.
add-exception-unverified-or-bad-signature-short = Unknown Identity
add-exception-unverified-or-bad-signature-long = The certificate is not trusted because it hasn’t been verified as issued by a trusted authority using a secure signature.
add-exception-valid-short = Valid Certificate
add-exception-valid-long = This site provides valid, verified identification.  There is no need to add an exception.
add-exception-checking-short = Checking Information
add-exception-checking-long = Attempting to identify this site…
add-exception-no-cert-short = No Information Available
add-exception-no-cert-long = Unable to obtain identification status for this site.

## Certificate export "Save as" and error dialogs

save-cert-as = Save Certificate To File
cert-format-base64 = X.509 Certificate (PEM)
cert-format-base64-chain = X.509 Certificate with chain (PEM)
cert-format-der = X.509 Certificate (DER)
cert-format-pkcs7 = X.509 Certificate (PKCS#7)
cert-format-pkcs7-chain = X.509 Certificate with chain (PKCS#7)
write-file-failure = File Error
