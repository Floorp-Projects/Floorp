# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import VARIABLE_REFERENCE
from fluent.migrate import COPY, REPLACE

def migrate(ctx):
	"""Bug 1561443 - Move _getErrorMessageFromCode from NetErrorChild.jsm to aboutNetError.js"""
	ctx.add_transforms(
                'browser/browser/nsserrors.ftl',
		'browser/browser/nsserrors.ftl',
	transforms_from(
"""
ssl-error-export-only-server = { COPY(from_path, "SSL_ERROR_EXPORT_ONLY_SERVER") }
ssl-error-us-only-server = { COPY(from_path, "SSL_ERROR_US_ONLY_SERVER") }
ssl-error-no-cypher-overlap = { COPY(from_path, "SSL_ERROR_NO_CYPHER_OVERLAP") }
ssl-error-no-certificate = { COPY(from_path, "SSL_ERROR_NO_CERTIFICATE") }
ssl-error-bad-certificate = { COPY(from_path, "SSL_ERROR_BAD_CERTIFICATE") }
ssl-error-bad-client = { COPY(from_path, "SSL_ERROR_BAD_CLIENT") }
ssl-error-bad-server = { COPY(from_path, "SSL_ERROR_BAD_SERVER") }
ssl-error-unsupported-certificate-type = { COPY(from_path, "SSL_ERROR_UNSUPPORTED_CERTIFICATE_TYPE") }
ssl-error-unsupported-version = { COPY(from_path, "SSL_ERROR_UNSUPPORTED_VERSION") }
ssl-error-wrong-certificate = { COPY(from_path, "SSL_ERROR_WRONG_CERTIFICATE") }
ssl-error-bad-cert-domain = { COPY(from_path, "SSL_ERROR_BAD_CERT_DOMAIN") }
ssl-error-post-warning = { COPY(from_path, "SSL_ERROR_POST_WARNING") }
ssl-error-ssl2-disabled = { COPY(from_path, "SSL_ERROR_SSL2_DISABLED") }
ssl-error-bad-mac-read = { COPY(from_path, "SSL_ERROR_BAD_MAC_READ") }
ssl-error-bad-mac-alert = { COPY(from_path, "SSL_ERROR_BAD_MAC_ALERT") }
ssl-error-bad-cert-alert = { COPY(from_path, "SSL_ERROR_BAD_CERT_ALERT") }
ssl-error-revoked-cert-alert = { COPY(from_path, "SSL_ERROR_REVOKED_CERT_ALERT") }
ssl-error-expired-cert-alert = { COPY(from_path, "SSL_ERROR_EXPIRED_CERT_ALERT") }
ssl-error-ssl-disabled = { COPY(from_path, "SSL_ERROR_SSL_DISABLED") }
ssl-error-fortezza-pqg = { COPY(from_path, "SSL_ERROR_FORTEZZA_PQG") }
ssl-error-unknown-cipher-suite = { COPY(from_path, "SSL_ERROR_UNKNOWN_CIPHER_SUITE") }
ssl-error-no-ciphers-supported = { COPY(from_path, "SSL_ERROR_NO_CIPHERS_SUPPORTED") }
ssl-error-bad-block-padding = { COPY(from_path, "SSL_ERROR_BAD_BLOCK_PADDING") }
ssl-error-rx-record-too-long = { COPY(from_path, "SSL_ERROR_RX_RECORD_TOO_LONG") }
ssl-error-tx-record-too-long = { COPY(from_path, "SSL_ERROR_TX_RECORD_TOO_LONG") }
ssl-error-rx-malformed-hello-request = { COPY(from_path, "SSL_ERROR_RX_MALFORMED_HELLO_REQUEST") }
ssl-error-rx-malformed-client-hello = { COPY(from_path, "SSL_ERROR_RX_MALFORMED_CLIENT_HELLO") }
ssl-error-rx-malformed-server-hello = { COPY(from_path, "SSL_ERROR_RX_MALFORMED_SERVER_HELLO") }
ssl-error-rx-malformed-certificate = { COPY(from_path, "SSL_ERROR_RX_MALFORMED_CERTIFICATE") }
ssl-error-rx-malformed-server-key-exch = { COPY(from_path, "SSL_ERROR_RX_MALFORMED_SERVER_KEY_EXCH") }
ssl-error-rx-malformed-cert-request = { COPY(from_path, "SSL_ERROR_RX_MALFORMED_CERT_REQUEST") }
ssl-error-rx-malformed-hello-done = { COPY(from_path, "SSL_ERROR_RX_MALFORMED_HELLO_DONE") }
ssl-error-rx-malformed-cert-verify = { COPY(from_path, "SSL_ERROR_RX_MALFORMED_CERT_VERIFY") }
ssl-error-rx-malformed-client-key-exch = { COPY(from_path, "SSL_ERROR_RX_MALFORMED_CLIENT_KEY_EXCH") }
ssl-error-rx-malformed-finished = { COPY(from_path, "SSL_ERROR_RX_MALFORMED_FINISHED") }
ssl-error-rx-malformed-change-cipher = { COPY(from_path, "SSL_ERROR_RX_MALFORMED_CHANGE_CIPHER") }
ssl-error-rx-malformed-alert = { COPY(from_path, "SSL_ERROR_RX_MALFORMED_ALERT") }
ssl-error-rx-malformed-handshake = { COPY(from_path, "SSL_ERROR_RX_MALFORMED_HANDSHAKE") }
ssl-error-rx-malformed-application-data = { COPY(from_path, "SSL_ERROR_RX_MALFORMED_APPLICATION_DATA") }
ssl-error-rx-unexpected-hello-request = { COPY(from_path, "SSL_ERROR_RX_UNEXPECTED_HELLO_REQUEST") }
ssl-error-rx-unexpected-client-hello = { COPY(from_path, "SSL_ERROR_RX_UNEXPECTED_CLIENT_HELLO") }
ssl-error-rx-unexpected-server-hello = { COPY(from_path, "SSL_ERROR_RX_UNEXPECTED_SERVER_HELLO") }
ssl-error-rx-unexpected-certificate = { COPY(from_path, "SSL_ERROR_RX_UNEXPECTED_CERTIFICATE") }
ssl-error-rx-unexpected-server-key-exch = { COPY(from_path, "SSL_ERROR_RX_UNEXPECTED_SERVER_KEY_EXCH") }
ssl-error-rx-unexpected-cert-request = { COPY(from_path, "SSL_ERROR_RX_UNEXPECTED_CERT_REQUEST") }
ssl-error-rx-unexpected-hello-done = { COPY(from_path, "SSL_ERROR_RX_UNEXPECTED_HELLO_DONE") }
ssl-error-rx-unexpected-cert-verify = { COPY(from_path, "SSL_ERROR_RX_UNEXPECTED_CERT_VERIFY") }
ssl-error-rx-unexpected-client-key-exch = { COPY(from_path, "SSL_ERROR_RX_UNEXPECTED_CLIENT_KEY_EXCH") }
ssl-error-rx-unexpected-finished = { COPY(from_path, "SSL_ERROR_RX_UNEXPECTED_FINISHED") }
ssl-error-rx-unexpected-change-cipher = { COPY(from_path, "SSL_ERROR_RX_UNEXPECTED_CHANGE_CIPHER") }
ssl-error-rx-unexpected-alert = { COPY(from_path, "SSL_ERROR_RX_UNEXPECTED_ALERT") }
ssl-error-rx-unexpected-handshake = { COPY(from_path, "SSL_ERROR_RX_UNEXPECTED_HANDSHAKE") }
ssl-error-rx-unexpected-application-data = { COPY(from_path, "SSL_ERROR_RX_UNEXPECTED_APPLICATION_DATA") }
ssl-error-rx-unknown-record-type = { COPY(from_path, "SSL_ERROR_RX_UNKNOWN_RECORD_TYPE") }
ssl-error-rx-unknown-handshake = { COPY(from_path, "SSL_ERROR_RX_UNKNOWN_HANDSHAKE") }
ssl-error-rx-unknown-alert = { COPY(from_path, "SSL_ERROR_RX_UNKNOWN_ALERT") }
ssl-error-close-notify-alert = { COPY(from_path, "SSL_ERROR_CLOSE_NOTIFY_ALERT") }
ssl-error-handshake-unexpected-alert = { COPY(from_path, "SSL_ERROR_HANDSHAKE_UNEXPECTED_ALERT") }
ssl-error-decompression-failure-alert = { COPY(from_path, "SSL_ERROR_DECOMPRESSION_FAILURE_ALERT") }
ssl-error-handshake-failure-alert = { COPY(from_path, "SSL_ERROR_HANDSHAKE_FAILURE_ALERT") }
ssl-error-illegal-parameter-alert = { COPY(from_path, "SSL_ERROR_ILLEGAL_PARAMETER_ALERT") }
ssl-error-unsupported-cert-alert = { COPY(from_path, "SSL_ERROR_UNSUPPORTED_CERT_ALERT") }
ssl-error-certificate-unknown-alert = { COPY(from_path, "SSL_ERROR_CERTIFICATE_UNKNOWN_ALERT") }
ssl-error-generate-random-failure = { COPY(from_path, "SSL_ERROR_GENERATE_RANDOM_FAILURE") }
ssl-error-sign-hashes-failure = { COPY(from_path, "SSL_ERROR_SIGN_HASHES_FAILURE") }
ssl-error-extract-public-key-failure = { COPY(from_path, "SSL_ERROR_EXTRACT_PUBLIC_KEY_FAILURE") }
ssl-error-server-key-exchange-failure = { COPY(from_path, "SSL_ERROR_SERVER_KEY_EXCHANGE_FAILURE") }
ssl-error-client-key-exchange-failure = { COPY(from_path, "SSL_ERROR_CLIENT_KEY_EXCHANGE_FAILURE") }
ssl-error-encryption-failure = { COPY(from_path, "SSL_ERROR_ENCRYPTION_FAILURE") }
ssl-error-decryption-failure = { COPY(from_path, "SSL_ERROR_DECRYPTION_FAILURE") }
ssl-error-socket-write-failure = { COPY(from_path, "SSL_ERROR_SOCKET_WRITE_FAILURE") }
ssl-error-md5-digest-failure = { COPY(from_path, "SSL_ERROR_MD5_DIGEST_FAILURE") }
ssl-error-sha-digest-failure = { COPY(from_path, "SSL_ERROR_SHA_DIGEST_FAILURE") }
ssl-error-mac-computation-failure = { COPY(from_path, "SSL_ERROR_MAC_COMPUTATION_FAILURE") }
ssl-error-sym-key-context-failure = { COPY(from_path, "SSL_ERROR_SYM_KEY_CONTEXT_FAILURE") }
ssl-error-sym-key-unwrap-failure = { COPY(from_path, "SSL_ERROR_SYM_KEY_UNWRAP_FAILURE") }
ssl-error-pub-key-size-limit-exceeded = { COPY(from_path, "SSL_ERROR_PUB_KEY_SIZE_LIMIT_EXCEEDED") }
ssl-error-iv-param-failure = { COPY(from_path, "SSL_ERROR_IV_PARAM_FAILURE") }
ssl-error-init-cipher-suite-failure = { COPY(from_path, "SSL_ERROR_INIT_CIPHER_SUITE_FAILURE") }
ssl-error-session-key-gen-failure = { COPY(from_path, "SSL_ERROR_SESSION_KEY_GEN_FAILURE") }
ssl-error-no-server-key-for-alg = { COPY(from_path, "SSL_ERROR_NO_SERVER_KEY_FOR_ALG") }
ssl-error-token-insertion-removal = { COPY(from_path, "SSL_ERROR_TOKEN_INSERTION_REMOVAL") }
ssl-error-token-slot-not-found = { COPY(from_path, "SSL_ERROR_TOKEN_SLOT_NOT_FOUND") }
ssl-error-no-compression-overlap = { COPY(from_path, "SSL_ERROR_NO_COMPRESSION_OVERLAP") }
ssl-error-handshake-not-completed = { COPY(from_path, "SSL_ERROR_HANDSHAKE_NOT_COMPLETED") }
ssl-error-bad-handshake-hash-value = { COPY(from_path, "SSL_ERROR_BAD_HANDSHAKE_HASH_VALUE") }
ssl-error-cert-kea-mismatch = { COPY(from_path, "SSL_ERROR_CERT_KEA_MISMATCH") }
ssl-error-no-trusted-ssl-client-ca = { COPY(from_path, "SSL_ERROR_NO_TRUSTED_SSL_CLIENT_CA") }
ssl-error-session-not-found = { COPY(from_path, "SSL_ERROR_SESSION_NOT_FOUND") }
ssl-error-decryption-failed-alert = { COPY(from_path, "SSL_ERROR_DECRYPTION_FAILED_ALERT") }
ssl-error-record-overflow-alert = { COPY(from_path, "SSL_ERROR_RECORD_OVERFLOW_ALERT") }
ssl-error-unknown-ca-alert = { COPY(from_path, "SSL_ERROR_UNKNOWN_CA_ALERT") }
ssl-error-access-denied-alert = { COPY(from_path, "SSL_ERROR_ACCESS_DENIED_ALERT") }
ssl-error-decode-error-alert = { COPY(from_path, "SSL_ERROR_DECODE_ERROR_ALERT") }
ssl-error-decrypt-error-alert = { COPY(from_path, "SSL_ERROR_DECRYPT_ERROR_ALERT") }
ssl-error-export-restriction-alert = { COPY(from_path, "SSL_ERROR_EXPORT_RESTRICTION_ALERT") }
ssl-error-protocol-version-alert = { COPY(from_path, "SSL_ERROR_PROTOCOL_VERSION_ALERT") }
ssl-error-insufficient-security-alert = { COPY(from_path, "SSL_ERROR_INSUFFICIENT_SECURITY_ALERT") }
ssl-error-internal-error-alert = { COPY(from_path, "SSL_ERROR_INTERNAL_ERROR_ALERT") }
ssl-error-user-canceled-alert = { COPY(from_path, "SSL_ERROR_USER_CANCELED_ALERT") }
ssl-error-no-renegotiation-alert = { COPY(from_path, "SSL_ERROR_NO_RENEGOTIATION_ALERT") }
ssl-error-server-cache-not-configured = { COPY(from_path, "SSL_ERROR_SERVER_CACHE_NOT_CONFIGURED") }
ssl-error-unsupported-extension-alert = { COPY(from_path, "SSL_ERROR_UNSUPPORTED_EXTENSION_ALERT") }
ssl-error-certificate-unobtainable-alert = { COPY(from_path, "SSL_ERROR_CERTIFICATE_UNOBTAINABLE_ALERT") }
ssl-error-unrecognized-name-alert = { COPY(from_path, "SSL_ERROR_UNRECOGNIZED_NAME_ALERT") }
ssl-error-bad-cert-status-response-alert = { COPY(from_path, "SSL_ERROR_BAD_CERT_STATUS_RESPONSE_ALERT") }
ssl-error-bad-cert-hash-value-alert = { COPY(from_path, "SSL_ERROR_BAD_CERT_HASH_VALUE_ALERT") }
ssl-error-rx-unexpected-new-session-ticket = { COPY(from_path, "SSL_ERROR_RX_UNEXPECTED_NEW_SESSION_TICKET") }
ssl-error-rx-malformed-new-session-ticket = { COPY(from_path, "SSL_ERROR_RX_MALFORMED_NEW_SESSION_TICKET") }
ssl-error-decompression-failure = { COPY(from_path, "SSL_ERROR_DECOMPRESSION_FAILURE") }
ssl-error-renegotiation-not-allowed = { COPY(from_path, "SSL_ERROR_RENEGOTIATION_NOT_ALLOWED") }
ssl-error-unsafe-negotiation = { COPY(from_path, "SSL_ERROR_UNSAFE_NEGOTIATION") }
ssl-error-rx-unexpected-uncompressed-record = { COPY(from_path, "SSL_ERROR_RX_UNEXPECTED_UNCOMPRESSED_RECORD") }
ssl-error-weak-server-ephemeral-dh-key = { COPY(from_path, "SSL_ERROR_WEAK_SERVER_EPHEMERAL_DH_KEY") }
ssl-error-next-protocol-data-invalid = { COPY(from_path, "SSL_ERROR_NEXT_PROTOCOL_DATA_INVALID") }
ssl-error-feature-not-supported-for-ssl2 = { COPY(from_path, "SSL_ERROR_FEATURE_NOT_SUPPORTED_FOR_SSL2") }
ssl-error-feature-not-supported-for-servers = { COPY(from_path, "SSL_ERROR_FEATURE_NOT_SUPPORTED_FOR_SERVERS") }
ssl-error-feature-not-supported-for-clients = { COPY(from_path, "SSL_ERROR_FEATURE_NOT_SUPPORTED_FOR_CLIENTS") }
ssl-error-invalid-version-range = { COPY(from_path, "SSL_ERROR_INVALID_VERSION_RANGE") }
ssl-error-cipher-disallowed-for-version = { COPY(from_path, "SSL_ERROR_CIPHER_DISALLOWED_FOR_VERSION") }
ssl-error-rx-malformed-hello-verify-request = { COPY(from_path, "SSL_ERROR_RX_MALFORMED_HELLO_VERIFY_REQUEST") }
ssl-error-rx-unexpected-hello-verify-request = { COPY(from_path, "SSL_ERROR_RX_UNEXPECTED_HELLO_VERIFY_REQUEST") }
ssl-error-feature-not-supported-for-version = { COPY(from_path, "SSL_ERROR_FEATURE_NOT_SUPPORTED_FOR_VERSION") }
ssl-error-rx-unexpected-cert-status = { COPY(from_path, "SSL_ERROR_RX_UNEXPECTED_CERT_STATUS") }
ssl-error-unsupported-hash-algorithm = { COPY(from_path, "SSL_ERROR_UNSUPPORTED_HASH_ALGORITHM") }
ssl-error-digest-failure = { COPY(from_path, "SSL_ERROR_DIGEST_FAILURE") }
ssl-error-incorrect-signature-algorithm = { COPY(from_path, "SSL_ERROR_INCORRECT_SIGNATURE_ALGORITHM") }
ssl-error-next-protocol-no-callback = { COPY(from_path, "SSL_ERROR_NEXT_PROTOCOL_NO_CALLBACK") }
ssl-error-next-protocol-no-protocol = { COPY(from_path, "SSL_ERROR_NEXT_PROTOCOL_NO_PROTOCOL") }
ssl-error-inappropriate-fallback-alert = { COPY(from_path, "SSL_ERROR_INAPPROPRIATE_FALLBACK_ALERT") }
ssl-error-weak-server-cert-key = { COPY(from_path, "SSL_ERROR_WEAK_SERVER_CERT_KEY") }
ssl-error-rx-short-dtls-read = { COPY(from_path, "SSL_ERROR_RX_SHORT_DTLS_READ") }
ssl-error-no-supported-signature-algorithm = { COPY(from_path, "SSL_ERROR_NO_SUPPORTED_SIGNATURE_ALGORITHM") }
ssl-error-unsupported-signature-algorithm = { COPY(from_path, "SSL_ERROR_UNSUPPORTED_SIGNATURE_ALGORITHM") }
ssl-error-missing-extended-master-secret = { COPY(from_path, "SSL_ERROR_MISSING_EXTENDED_MASTER_SECRET") }
ssl-error-unexpected-extended-master-secret = { COPY(from_path, "SSL_ERROR_UNEXPECTED_EXTENDED_MASTER_SECRET") }
sec-error-io = { COPY(from_path, "SEC_ERROR_IO") }
sec-error-library-failure = { COPY(from_path, "SEC_ERROR_LIBRARY_FAILURE") }
sec-error-bad-data = { COPY(from_path, "SEC_ERROR_BAD_DATA") }
sec-error-output-len = { COPY(from_path, "SEC_ERROR_OUTPUT_LEN") }
sec-error-input-len = { COPY(from_path, "SEC_ERROR_INPUT_LEN") }
sec-error-invalid-args = { COPY(from_path, "SEC_ERROR_INVALID_ARGS") }
sec-error-invalid-algorithm = { COPY(from_path, "SEC_ERROR_INVALID_ALGORITHM") }
sec-error-invalid-ava = { COPY(from_path, "SEC_ERROR_INVALID_AVA") }
sec-error-invalid-time = { COPY(from_path, "SEC_ERROR_INVALID_TIME") }
sec-error-bad-der = { COPY(from_path, "SEC_ERROR_BAD_DER") }
sec-error-bad-signature = { COPY(from_path, "SEC_ERROR_BAD_SIGNATURE") }
sec-error-expired-certificate = { COPY(from_path, "SEC_ERROR_EXPIRED_CERTIFICATE") }
sec-error-revoked-certificate = { COPY(from_path, "SEC_ERROR_REVOKED_CERTIFICATE") }
sec-error-unknown-issuer = { COPY(from_path, "SEC_ERROR_UNKNOWN_ISSUER") }
sec-error-bad-key = { COPY(from_path, "SEC_ERROR_BAD_KEY") }
sec-error-bad-password = { COPY(from_path, "SEC_ERROR_BAD_PASSWORD") }
sec-error-retry-password = { COPY(from_path, "SEC_ERROR_RETRY_PASSWORD") }
sec-error-no-nodelock = { COPY(from_path, "SEC_ERROR_NO_NODELOCK") }
sec-error-bad-database = { COPY(from_path, "SEC_ERROR_BAD_DATABASE") }
sec-error-no-memory = { COPY(from_path, "SEC_ERROR_NO_MEMORY") }
sec-error-untrusted-issuer = { COPY(from_path, "SEC_ERROR_UNTRUSTED_ISSUER") }
sec-error-untrusted-cert = { COPY(from_path, "SEC_ERROR_UNTRUSTED_CERT") }
sec-error-duplicate-cert = { COPY(from_path, "SEC_ERROR_DUPLICATE_CERT") }
sec-error-duplicate-cert-name = { COPY(from_path, "SEC_ERROR_DUPLICATE_CERT_NAME") }
sec-error-adding-cert = { COPY(from_path, "SEC_ERROR_ADDING_CERT") }
sec-error-filing-key = { COPY(from_path, "SEC_ERROR_FILING_KEY") }
sec-error-no-key = { COPY(from_path, "SEC_ERROR_NO_KEY") }
sec-error-cert-valid = { COPY(from_path, "SEC_ERROR_CERT_VALID") }
sec-error-cert-not-valid = { COPY(from_path, "SEC_ERROR_CERT_NOT_VALID") }
sec-error-cert-no-response = { COPY(from_path, "SEC_ERROR_CERT_NO_RESPONSE") }
sec-error-expired-issuer-certificate = { COPY(from_path, "SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE") }
sec-error-crl-expired = { COPY(from_path, "SEC_ERROR_CRL_EXPIRED") }
sec-error-crl-bad-signature = { COPY(from_path, "SEC_ERROR_CRL_BAD_SIGNATURE") }
sec-error-crl-invalid = { COPY(from_path, "SEC_ERROR_CRL_INVALID") }
sec-error-extension-value-invalid = { COPY(from_path, "SEC_ERROR_EXTENSION_VALUE_INVALID") }
sec-error-extension-not-found = { COPY(from_path, "SEC_ERROR_EXTENSION_NOT_FOUND") }
sec-error-ca-cert-invalid = { COPY(from_path, "SEC_ERROR_CA_CERT_INVALID") }
sec-error-path-len-constraint-invalid = { COPY(from_path, "SEC_ERROR_PATH_LEN_CONSTRAINT_INVALID") }
sec-error-cert-usages-invalid = { COPY(from_path, "SEC_ERROR_CERT_USAGES_INVALID") }
sec-internal-only = { COPY(from_path, "SEC_INTERNAL_ONLY") }
sec-error-invalid-key = { COPY(from_path, "SEC_ERROR_INVALID_KEY") }
sec-error-unknown-critical-extension = { COPY(from_path, "SEC_ERROR_UNKNOWN_CRITICAL_EXTENSION") }
sec-error-old-crl = { COPY(from_path, "SEC_ERROR_OLD_CRL") }
sec-error-no-email-cert = { COPY(from_path, "SEC_ERROR_NO_EMAIL_CERT") }
sec-error-no-recipient-certs-query = { COPY(from_path, "SEC_ERROR_NO_RECIPIENT_CERTS_QUERY") }
sec-error-not-a-recipient = { COPY(from_path, "SEC_ERROR_NOT_A_RECIPIENT") }
sec-error-pkcs7-keyalg-mismatch = { COPY(from_path, "SEC_ERROR_PKCS7_KEYALG_MISMATCH") }
sec-error-pkcs7-bad-signature = { COPY(from_path, "SEC_ERROR_PKCS7_BAD_SIGNATURE") }
sec-error-unsupported-keyalg = { COPY(from_path, "SEC_ERROR_UNSUPPORTED_KEYALG") }
sec-error-decryption-disallowed = { COPY(from_path, "SEC_ERROR_DECRYPTION_DISALLOWED") }
xp-sec-fortezza-bad-card = { COPY(from_path, "XP_SEC_FORTEZZA_BAD_CARD") }
xp-sec-fortezza-no-card = { COPY(from_path, "XP_SEC_FORTEZZA_NO_CARD") }
xp-sec-fortezza-none-selected = { COPY(from_path, "XP_SEC_FORTEZZA_NONE_SELECTED") }
xp-sec-fortezza-more-info = { COPY(from_path, "XP_SEC_FORTEZZA_MORE_INFO") }
xp-sec-fortezza-person-not-found = { COPY(from_path, "XP_SEC_FORTEZZA_PERSON_NOT_FOUND") }
xp-sec-fortezza-no-more-info = { COPY(from_path, "XP_SEC_FORTEZZA_NO_MORE_INFO") }
xp-sec-fortezza-bad-pin = { COPY(from_path, "XP_SEC_FORTEZZA_BAD_PIN") }
xp-sec-fortezza-person-error = { COPY(from_path, "XP_SEC_FORTEZZA_PERSON_ERROR") }
sec-error-no-krl = { COPY(from_path, "SEC_ERROR_NO_KRL") }
sec-error-krl-expired = { COPY(from_path, "SEC_ERROR_KRL_EXPIRED") }
sec-error-krl-bad-signature = { COPY(from_path, "SEC_ERROR_KRL_BAD_SIGNATURE") }
sec-error-revoked-key = { COPY(from_path, "SEC_ERROR_REVOKED_KEY") }
sec-error-krl-invalid = { COPY(from_path, "SEC_ERROR_KRL_INVALID") }
sec-error-need-random = { COPY(from_path, "SEC_ERROR_NEED_RANDOM") }
sec-error-no-module = { COPY(from_path, "SEC_ERROR_NO_MODULE") }
sec-error-no-token = { COPY(from_path, "SEC_ERROR_NO_TOKEN") }
sec-error-read-only = { COPY(from_path, "SEC_ERROR_READ_ONLY") }
sec-error-no-slot-selected = { COPY(from_path, "SEC_ERROR_NO_SLOT_SELECTED") }
sec-error-cert-nickname-collision = { COPY(from_path, "SEC_ERROR_CERT_NICKNAME_COLLISION") }
sec-error-key-nickname-collision = { COPY(from_path, "SEC_ERROR_KEY_NICKNAME_COLLISION") }
sec-error-safe-not-created = { COPY(from_path, "SEC_ERROR_SAFE_NOT_CREATED") }
sec-error-baggage-not-created = { COPY(from_path, "SEC_ERROR_BAGGAGE_NOT_CREATED") }
xp-java-remove-principal-error = { COPY(from_path, "XP_JAVA_REMOVE_PRINCIPAL_ERROR") }
xp-java-delete-privilege-error = { COPY(from_path, "XP_JAVA_DELETE_PRIVILEGE_ERROR") }
xp-java-cert-not-exists-error = { COPY(from_path, "XP_JAVA_CERT_NOT_EXISTS_ERROR") }
sec-error-bad-export-algorithm = { COPY(from_path, "SEC_ERROR_BAD_EXPORT_ALGORITHM") }
sec-error-exporting-certificates = { COPY(from_path, "SEC_ERROR_EXPORTING_CERTIFICATES") }
sec-error-importing-certificates = { COPY(from_path, "SEC_ERROR_IMPORTING_CERTIFICATES") }
sec-error-pkcs12-decoding-pfx = { COPY(from_path, "SEC_ERROR_PKCS12_DECODING_PFX") }
sec-error-pkcs12-invalid-mac = { COPY(from_path, "SEC_ERROR_PKCS12_INVALID_MAC") }
sec-error-pkcs12-unsupported-mac-algorithm = { COPY(from_path, "SEC_ERROR_PKCS12_UNSUPPORTED_MAC_ALGORITHM") }
sec-error-pkcs12-unsupported-transport-mode = { COPY(from_path, "SEC_ERROR_PKCS12_UNSUPPORTED_TRANSPORT_MODE") }
sec-error-pkcs12-corrupt-pfx-structure = { COPY(from_path, "SEC_ERROR_PKCS12_CORRUPT_PFX_STRUCTURE") }
sec-error-pkcs12-unsupported-pbe-algorithm = { COPY(from_path, "SEC_ERROR_PKCS12_UNSUPPORTED_PBE_ALGORITHM") }
sec-error-pkcs12-unsupported-version = { COPY(from_path, "SEC_ERROR_PKCS12_UNSUPPORTED_VERSION") }
sec-error-pkcs12-privacy-password-incorrect = { COPY(from_path, "SEC_ERROR_PKCS12_PRIVACY_PASSWORD_INCORRECT") }
sec-error-pkcs12-cert-collision = { COPY(from_path, "SEC_ERROR_PKCS12_CERT_COLLISION") }
sec-error-user-cancelled = { COPY(from_path, "SEC_ERROR_USER_CANCELLED") }
sec-error-pkcs12-duplicate-data = { COPY(from_path, "SEC_ERROR_PKCS12_DUPLICATE_DATA") }
sec-error-message-send-aborted = { COPY(from_path, "SEC_ERROR_MESSAGE_SEND_ABORTED") }
sec-error-inadequate-key-usage = { COPY(from_path, "SEC_ERROR_INADEQUATE_KEY_USAGE") }
sec-error-inadequate-cert-type = { COPY(from_path, "SEC_ERROR_INADEQUATE_CERT_TYPE") }
sec-error-cert-addr-mismatch = { COPY(from_path, "SEC_ERROR_CERT_ADDR_MISMATCH") }
sec-error-pkcs12-unable-to-import-key = { COPY(from_path, "SEC_ERROR_PKCS12_UNABLE_TO_IMPORT_KEY") }
sec-error-pkcs12-importing-cert-chain = { COPY(from_path, "SEC_ERROR_PKCS12_IMPORTING_CERT_CHAIN") }
sec-error-pkcs12-unable-to-locate-object-by-name = { COPY(from_path, "SEC_ERROR_PKCS12_UNABLE_TO_LOCATE_OBJECT_BY_NAME") }
sec-error-pkcs12-unable-to-export-key = { COPY(from_path, "SEC_ERROR_PKCS12_UNABLE_TO_EXPORT_KEY") }
sec-error-pkcs12-unable-to-write = { COPY(from_path, "SEC_ERROR_PKCS12_UNABLE_TO_WRITE") }
sec-error-pkcs12-unable-to-read = { COPY(from_path, "SEC_ERROR_PKCS12_UNABLE_TO_READ") }
sec-error-pkcs12-key-database-not-initialized = { COPY(from_path, "SEC_ERROR_PKCS12_KEY_DATABASE_NOT_INITIALIZED") }
sec-error-keygen-fail = { COPY(from_path, "SEC_ERROR_KEYGEN_FAIL") }
sec-error-invalid-password = { COPY(from_path, "SEC_ERROR_INVALID_PASSWORD") }
sec-error-retry-old-password = { COPY(from_path, "SEC_ERROR_RETRY_OLD_PASSWORD") }
sec-error-bad-nickname = { COPY(from_path, "SEC_ERROR_BAD_NICKNAME") }
sec-error-not-fortezza-issuer = { COPY(from_path, "SEC_ERROR_NOT_FORTEZZA_ISSUER") }
sec-error-cannot-move-sensitive-key = { COPY(from_path, "SEC_ERROR_CANNOT_MOVE_SENSITIVE_KEY") }
sec-error-js-invalid-module-name = { COPY(from_path, "SEC_ERROR_JS_INVALID_MODULE_NAME") }
sec-error-js-invalid-dll = { COPY(from_path, "SEC_ERROR_JS_INVALID_DLL") }
sec-error-js-add-mod-failure = { COPY(from_path, "SEC_ERROR_JS_ADD_MOD_FAILURE") }
sec-error-js-del-mod-failure = { COPY(from_path, "SEC_ERROR_JS_DEL_MOD_FAILURE") }
sec-error-old-krl = { COPY(from_path, "SEC_ERROR_OLD_KRL") }
sec-error-ckl-conflict = { COPY(from_path, "SEC_ERROR_CKL_CONFLICT") }
sec-error-cert-not-in-name-space = { COPY(from_path, "SEC_ERROR_CERT_NOT_IN_NAME_SPACE") }
sec-error-krl-not-yet-valid = { COPY(from_path, "SEC_ERROR_KRL_NOT_YET_VALID") }
sec-error-crl-not-yet-valid = { COPY(from_path, "SEC_ERROR_CRL_NOT_YET_VALID") }
sec-error-unknown-cert = { COPY(from_path, "SEC_ERROR_UNKNOWN_CERT") }
sec-error-unknown-signer = { COPY(from_path, "SEC_ERROR_UNKNOWN_SIGNER") }
sec-error-cert-bad-access-location = { COPY(from_path, "SEC_ERROR_CERT_BAD_ACCESS_LOCATION") }
sec-error-ocsp-unknown-response-type = { COPY(from_path, "SEC_ERROR_OCSP_UNKNOWN_RESPONSE_TYPE") }
sec-error-ocsp-bad-http-response = { COPY(from_path, "SEC_ERROR_OCSP_BAD_HTTP_RESPONSE") }
sec-error-ocsp-malformed-request = { COPY(from_path, "SEC_ERROR_OCSP_MALFORMED_REQUEST") }
sec-error-ocsp-server-error = { COPY(from_path, "SEC_ERROR_OCSP_SERVER_ERROR") }
sec-error-ocsp-try-server-later = { COPY(from_path, "SEC_ERROR_OCSP_TRY_SERVER_LATER") }
sec-error-ocsp-request-needs-sig = { COPY(from_path, "SEC_ERROR_OCSP_REQUEST_NEEDS_SIG") }
sec-error-ocsp-unauthorized-request = { COPY(from_path, "SEC_ERROR_OCSP_UNAUTHORIZED_REQUEST") }
sec-error-ocsp-unknown-response-status = { COPY(from_path, "SEC_ERROR_OCSP_UNKNOWN_RESPONSE_STATUS") }
sec-error-ocsp-unknown-cert = { COPY(from_path, "SEC_ERROR_OCSP_UNKNOWN_CERT") }
sec-error-ocsp-not-enabled = { COPY(from_path, "SEC_ERROR_OCSP_NOT_ENABLED") }
sec-error-ocsp-no-default-responder = { COPY(from_path, "SEC_ERROR_OCSP_NO_DEFAULT_RESPONDER") }
sec-error-ocsp-malformed-response = { COPY(from_path, "SEC_ERROR_OCSP_MALFORMED_RESPONSE") }
sec-error-ocsp-unauthorized-response = { COPY(from_path, "SEC_ERROR_OCSP_UNAUTHORIZED_RESPONSE") }
sec-error-ocsp-future-response = { COPY(from_path, "SEC_ERROR_OCSP_FUTURE_RESPONSE") }
sec-error-ocsp-old-response = { COPY(from_path, "SEC_ERROR_OCSP_OLD_RESPONSE") }
sec-error-digest-not-found = { COPY(from_path, "SEC_ERROR_DIGEST_NOT_FOUND") }
sec-error-unsupported-message-type = { COPY(from_path, "SEC_ERROR_UNSUPPORTED_MESSAGE_TYPE") }
sec-error-module-stuck = { COPY(from_path, "SEC_ERROR_MODULE_STUCK") }
sec-error-bad-template = { COPY(from_path, "SEC_ERROR_BAD_TEMPLATE") }
sec-error-crl-not-found = { COPY(from_path, "SEC_ERROR_CRL_NOT_FOUND") }
sec-error-reused-issuer-and-serial = { COPY(from_path, "SEC_ERROR_REUSED_ISSUER_AND_SERIAL") }
sec-error-busy = { COPY(from_path, "SEC_ERROR_BUSY") }
sec-error-extra-input = { COPY(from_path, "SEC_ERROR_EXTRA_INPUT") }
sec-error-unsupported-elliptic-curve = { COPY(from_path, "SEC_ERROR_UNSUPPORTED_ELLIPTIC_CURVE") }
sec-error-unsupported-ec-point-form = { COPY(from_path, "SEC_ERROR_UNSUPPORTED_EC_POINT_FORM") }
sec-error-unrecognized-oid = { COPY(from_path, "SEC_ERROR_UNRECOGNIZED_OID") }
sec-error-ocsp-invalid-signing-cert = { COPY(from_path, "SEC_ERROR_OCSP_INVALID_SIGNING_CERT") }
sec-error-revoked-certificate-crl = { COPY(from_path, "SEC_ERROR_REVOKED_CERTIFICATE_CRL") }
sec-error-revoked-certificate-ocsp = { COPY(from_path, "SEC_ERROR_REVOKED_CERTIFICATE_OCSP") }
sec-error-crl-invalid-version = { COPY(from_path, "SEC_ERROR_CRL_INVALID_VERSION") }
sec-error-crl-v1-critical-extension = { COPY(from_path, "SEC_ERROR_CRL_V1_CRITICAL_EXTENSION") }
sec-error-crl-unknown-critical-extension = { COPY(from_path, "SEC_ERROR_CRL_UNKNOWN_CRITICAL_EXTENSION") }
sec-error-unknown-object-type = { COPY(from_path, "SEC_ERROR_UNKNOWN_OBJECT_TYPE") }
sec-error-incompatible-pkcs11 = { COPY(from_path, "SEC_ERROR_INCOMPATIBLE_PKCS11") }
sec-error-no-event = { COPY(from_path, "SEC_ERROR_NO_EVENT") }
sec-error-crl-already-exists = { COPY(from_path, "SEC_ERROR_CRL_ALREADY_EXISTS") }
sec-error-not-initialized = { COPY(from_path, "SEC_ERROR_NOT_INITIALIZED") }
sec-error-token-not-logged-in = { COPY(from_path, "SEC_ERROR_TOKEN_NOT_LOGGED_IN") }
sec-error-ocsp-responder-cert-invalid = { COPY(from_path, "SEC_ERROR_OCSP_RESPONDER_CERT_INVALID") }
sec-error-ocsp-bad-signature = { COPY(from_path, "SEC_ERROR_OCSP_BAD_SIGNATURE") }
sec-error-out-of-search-limits = { COPY(from_path, "SEC_ERROR_OUT_OF_SEARCH_LIMITS") }
sec-error-invalid-policy-mapping = { COPY(from_path, "SEC_ERROR_INVALID_POLICY_MAPPING") }
sec-error-policy-validation-failed = { COPY(from_path, "SEC_ERROR_POLICY_VALIDATION_FAILED") }
sec-error-unknown-aia-location-type = { COPY(from_path, "SEC_ERROR_UNKNOWN_AIA_LOCATION_TYPE") }
sec-error-bad-http-response = { COPY(from_path, "SEC_ERROR_BAD_HTTP_RESPONSE") }
sec-error-bad-ldap-response = { COPY(from_path, "SEC_ERROR_BAD_LDAP_RESPONSE") }
sec-error-failed-to-encode-data = { COPY(from_path, "SEC_ERROR_FAILED_TO_ENCODE_DATA") }
sec-error-bad-info-access-location = { COPY(from_path, "SEC_ERROR_BAD_INFO_ACCESS_LOCATION") }
sec-error-libpkix-internal = { COPY(from_path, "SEC_ERROR_LIBPKIX_INTERNAL") }
sec-error-pkcs11-general-error = { COPY(from_path, "SEC_ERROR_PKCS11_GENERAL_ERROR") }
sec-error-pkcs11-function-failed = { COPY(from_path, "SEC_ERROR_PKCS11_FUNCTION_FAILED") }
sec-error-pkcs11-device-error = { COPY(from_path, "SEC_ERROR_PKCS11_DEVICE_ERROR") }
sec-error-bad-info-access-method = { COPY(from_path, "SEC_ERROR_BAD_INFO_ACCESS_METHOD") }
sec-error-crl-import-failed = { COPY(from_path, "SEC_ERROR_CRL_IMPORT_FAILED") }
sec-error-expired-password = { COPY(from_path, "SEC_ERROR_EXPIRED_PASSWORD") }
sec-error-locked-password = { COPY(from_path, "SEC_ERROR_LOCKED_PASSWORD") }
sec-error-unknown-pkcs11-error = { COPY(from_path, "SEC_ERROR_UNKNOWN_PKCS11_ERROR") }
sec-error-bad-crl-dp-url = { COPY(from_path, "SEC_ERROR_BAD_CRL_DP_URL") }
sec-error-cert-signature-algorithm-disabled = { COPY(from_path, "SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED") }
mozilla-pkix-error-key-pinning-failure = { COPY(from_path, "MOZILLA_PKIX_ERROR_KEY_PINNING_FAILURE") }
mozilla-pkix-error-ca-cert-used-as-end-entity = { COPY(from_path, "MOZILLA_PKIX_ERROR_CA_CERT_USED_AS_END_ENTITY") }
mozilla-pkix-error-inadequate-key-size = { COPY(from_path, "MOZILLA_PKIX_ERROR_INADEQUATE_KEY_SIZE") }
mozilla-pkix-error-v1-cert-used-as-ca = { COPY(from_path, "MOZILLA_PKIX_ERROR_V1_CERT_USED_AS_CA") }
mozilla-pkix-error-not-yet-valid-certificate = { COPY(from_path, "MOZILLA_PKIX_ERROR_NOT_YET_VALID_CERTIFICATE") }
mozilla-pkix-error-not-yet-valid-issuer-certificate = { COPY(from_path, "MOZILLA_PKIX_ERROR_NOT_YET_VALID_ISSUER_CERTIFICATE") }
mozilla-pkix-error-signature-algorithm-mismatch = { COPY(from_path, "MOZILLA_PKIX_ERROR_SIGNATURE_ALGORITHM_MISMATCH") }
mozilla-pkix-error-ocsp-response-for-cert-missing = { COPY(from_path, "MOZILLA_PKIX_ERROR_OCSP_RESPONSE_FOR_CERT_MISSING") }
mozilla-pkix-error-validity-too-long = { COPY(from_path, "MOZILLA_PKIX_ERROR_VALIDITY_TOO_LONG") }
mozilla-pkix-error-required-tls-feature-missing = { COPY(from_path, "MOZILLA_PKIX_ERROR_REQUIRED_TLS_FEATURE_MISSING") }
mozilla-pkix-error-invalid-integer-encoding = { COPY(from_path, "MOZILLA_PKIX_ERROR_INVALID_INTEGER_ENCODING") }
mozilla-pkix-error-empty-issuer-name = { COPY(from_path, "MOZILLA_PKIX_ERROR_EMPTY_ISSUER_NAME") }
mozilla-pkix-error-additional-policy-constraint-failed = { COPY(from_path, "MOZILLA_PKIX_ERROR_ADDITIONAL_POLICY_CONSTRAINT_FAILED") }
mozilla-pkix-error-self-signed-cert = { COPY(from_path, "MOZILLA_PKIX_ERROR_SELF_SIGNED_CERT") }
""", from_path="security/manager/chrome/pipnss/nsserrors.properties"))
	ctx.add_transforms(
		'browser/browser/nsserrors.ftl',
		'browser/browser/nsserrors.ftl',
		[
			FTL.Message(
				id=FTL.Identifier('ssl-connection-error'),
				value=REPLACE(
					'security/manager/chrome/pipnss/pipnss.properties',
					'SSLConnectionErrorPrefix2',
					{
						"%1$S": VARIABLE_REFERENCE("hostname"),
						"%2$S": VARIABLE_REFERENCE("errorMessage"),
						"\n": FTL.TextElement(""),
					},
					normalize_printf=True
				),
			),
			FTL.Message(
				id=FTL.Identifier('cert-error-code-prefix'),
				value=REPLACE(
					'security/manager/chrome/pipnss/pipnss.properties',
					'certErrorCodePrefix3',
					{
						"%1$S": VARIABLE_REFERENCE("error"),
					},
					normalize_printf=True
                                ),
                        ),
		]
	)
	ctx.add_transforms(
                'browser/browser/nsserrors.ftl',
                'browser/browser/nsserrors.ftl',
	transforms_from(
"""
psmerr-ssl-disabled = { COPY(from_path, "PSMERR_SSL_Disabled") }
psmerr-ssl2-disabled = { COPY(from_path, "PSMERR_SSL2_Disabled") }
psmerr-hostreusedissuerandserial = { COPY(from_path, "PSMERR_HostReusedIssuerSerial") }
""", from_path="security/manager/chrome/pipnss/pipnss.properties"))
