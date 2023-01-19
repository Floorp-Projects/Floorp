.. _mozilla_projects_nss_ssl_functions_sslerr:

sslerr
======

.. container::

   .. note::

      -  This page is part of the :ref:`mozilla_projects_nss_ssl_functions_old_ssl_reference` that
         we are migrating into the format described in the `MDN Style
         Guide <https://developer.mozilla.org/en-US/docs/Project:MDC_style_guide>`__. If you are
         inclined to help with this migration, your help would be very much appreciated.

      -  Upgraded documentation may be found in the :ref:`mozilla_projects_nss_reference`

   .. rubric:: NSS and SSL Error Codes
      :name: NSS_and_SSL_Error_Codes

   --------------

.. _chapter_8_nss_and_ssl_error_codes:

`Chapter 8
 <#chapter_8_nss_and_ssl_error_codes>`__\ NSS and SSL Error Codes
-----------------------------------------------------------------

.. container::

   NSS error codes are retrieved using the NSPR function
   `PR_GetError <../../../../../nspr/reference/html/prerr.html#PR_GetError>`__. In addition to the
   `error codes defined by
   NSPR <https://dxr.mozilla.org/mozilla-central/source/nsprpub/pr/include/prerr.h>`__, PR_GetError
   retrieves the error codes described in this chapter.

   | `SSL Error Codes <#1040263>`__
   | `SEC Error Codes <#1039257>`__

.. _ssl_error_codes:

`SSL Error Codes <#ssl_error_codes>`__
--------------------------------------

.. container::

   **Table 8.1 Error codes defined in sslerr.h**

   +--------------------------------+--------------------------------+--------------------------------+
   | **Constant**                   | **Value**                      | **Description**                |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_EXPORT_ONLY_SERVER   | -12288                         | "Unable to communicate         |
   |                                |                                | securely. Peer does not        |
   |                                |                                | support high-grade             |
   |                                |                                | encryption."                   |
   |                                |                                |                                |
   |                                |                                | The local system was           |
   |                                |                                | configured to support the      |
   |                                |                                | cipher suites permitted for    |
   |                                |                                | domestic use. The remote       |
   |                                |                                | system was configured to       |
   |                                |                                | support only the cipher suites |
   |                                |                                | permitted for export use.      |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_US_ONLY_SERVER       | -12287                         | "Unable to communicate         |
   |                                |                                | securely. Peer requires        |
   |                                |                                | high-grade encryption which is |
   |                                |                                | not supported."                |
   |                                |                                |                                |
   |                                |                                | The remote system was          |
   |                                |                                | configured to support the      |
   |                                |                                | cipher suites permitted for    |
   |                                |                                | domestic use. The local system |
   |                                |                                | was configured to support only |
   |                                |                                | the cipher suites permitted    |
   |                                |                                | for export use.                |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_NO_CYPHER_OVERLAP    | -12286                         | "Cannot communicate securely   |
   |                                |                                | with peer: no common           |
   |                                |                                | encryption algorithm(s)."      |
   |                                |                                |                                |
   |                                |                                | The local and remote systems   |
   |                                |                                | share no cipher suites in      |
   |                                |                                | common. This can be due to a   |
   |                                |                                | misconfiguration at either     |
   |                                |                                | end. It can be due to a server |
   |                                |                                | being misconfigured to use a   |
   |                                |                                | non-RSA certificate with the   |
   |                                |                                | RSA key exchange algorithm.    |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_NO_CERTIFICATE       | -12285                         | "Unable to find the            |
   |                                |                                | certificate or key necessary   |
   |                                |                                | for authentication."           |
   |                                |                                |                                |
   |                                |                                | This error has many potential  |
   |                                |                                | causes; for example:           |
   |                                |                                |                                |
   |                                |                                | Certificate or key not found   |
   |                                |                                | in database.                   |
   |                                |                                |                                |
   |                                |                                | Certificate not marked trusted |
   |                                |                                | in database and Certificate's  |
   |                                |                                | issuer not marked trusted in   |
   |                                |                                | database.                      |
   |                                |                                |                                |
   |                                |                                | Wrong password for key         |
   |                                |                                | database.                      |
   |                                |                                |                                |
   |                                |                                | Missing database.              |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_BAD_CERTIFICATE      | -12284                         | "Unable to communicate         |
   |                                |                                | securely with peer: peers's    |
   |                                |                                | certificate was rejected."     |
   |                                |                                |                                |
   |                                |                                | A certificate was received     |
   |                                |                                | from the remote system and was |
   |                                |                                | passed to the certificate      |
   |                                |                                | authentication callback        |
   |                                |                                | function provided by the local |
   |                                |                                | application. That callback     |
   |                                |                                | function returned SECFailure,  |
   |                                |                                | and the bad certificate        |
   |                                |                                | callback function either was   |
   |                                |                                | not configured or did not      |
   |                                |                                | choose to override the error   |
   |                                |                                | code returned by the           |
   |                                |                                | certificate authentication     |
   |                                |                                | callback function.             |
   +--------------------------------+--------------------------------+--------------------------------+
   |                                | -12283                         | (unused)                       |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_BAD_CLIENT           | -12282                         | "The server has encountered    |
   |                                |                                | bad data from the client."     |
   |                                |                                |                                |
   |                                |                                | This error code should occur   |
   |                                |                                | only on sockets that are       |
   |                                |                                | acting as servers. It is a     |
   |                                |                                | generic error, used when none  |
   |                                |                                | of the other more specific     |
   |                                |                                | error codes defined in this    |
   |                                |                                | file applies.                  |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_BAD_SERVER           | -12281                         | "The client has encountered    |
   |                                |                                | bad data from the server."     |
   |                                |                                |                                |
   |                                |                                | This error code should occur   |
   |                                |                                | only on sockets that are       |
   |                                |                                | acting as clients. It is a     |
   |                                |                                | generic error, used when none  |
   |                                |                                | of the other more specific     |
   |                                |                                | error codes defined in this    |
   |                                |                                | file applies.                  |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERRO                       | -12280                         | "Unsupported certificate       |
   | R_UNSUPPORTED_CERTIFICATE_TYPE |                                | type."                         |
   |                                |                                |                                |
   |                                |                                | The operation encountered a    |
   |                                |                                | certificate that was not one   |
   |                                |                                | of the well known certificate  |
   |                                |                                | types handled by the           |
   |                                |                                | certificate library.           |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_UNSUPPORTED_VERSION  | -12279                         | "Peer using unsupported        |
   |                                |                                | version of security protocol." |
   |                                |                                |                                |
   |                                |                                | On a client socket, this means |
   |                                |                                | the remote server has          |
   |                                |                                | attempted to negotiate the use |
   |                                |                                | of a version of SSL that is    |
   |                                |                                | not supported by the NSS       |
   |                                |                                | library, probably an invalid   |
   |                                |                                | version number. On a server    |
   |                                |                                | socket, this means the remote  |
   |                                |                                | client has requested the use   |
   |                                |                                | of a version of SSL older than |
   |                                |                                | version 2.                     |
   +--------------------------------+--------------------------------+--------------------------------+
   |                                | -12278                         | (unused)                       |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_WRONG_CERTIFICATE    | -12277                         | "Client authentication failed: |
   |                                |                                | private key in key database    |
   |                                |                                | does not correspond to public  |
   |                                |                                | key in certificate database."  |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_BAD_CERT_DOMAIN      | -12276                         | "Unable to communicate         |
   |                                |                                | securely with peer: requested  |
   |                                |                                | domain name does not match the |
   |                                |                                | server's certificate."         |
   |                                |                                |                                |
   |                                |                                | This error code should be      |
   |                                |                                | returned by the certificate    |
   |                                |                                | authentication callback        |
   |                                |                                | function when it detects that  |
   |                                |                                | the Common Name in the remote  |
   |                                |                                | server's certificate does not  |
   |                                |                                | match the hostname sought by   |
   |                                |                                | the local client, according to |
   |                                |                                | the matching rules specified   |
   |                                |                                | for                            |
   |                                |                                | `CERT_VerifyCertN              |
   |                                |                                | ame <sslcrt.html#1050342>`__.  |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_POST_WARNING         | -12275                         | (unused)                       |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_SSL2_DISABLED        | -12274                         | "Peer only supports SSL        |
   |                                |                                | version 2, which is locally    |
   |                                |                                | disabled."                     |
   |                                |                                |                                |
   |                                |                                | The remote server has asked to |
   |                                |                                | use SSL version 2, and SSL     |
   |                                |                                | version 2 is disabled in the   |
   |                                |                                | local client's configuration.  |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_BAD_MAC_READ         | -12273                         | "SSL received a record with an |
   |                                |                                | incorrect Message              |
   |                                |                                | Authentication Code."          |
   |                                |                                |                                |
   |                                |                                | This usually indicates that    |
   |                                |                                | the client and server have     |
   |                                |                                | failed to come to agreement on |
   |                                |                                | the set of keys used to        |
   |                                |                                | encrypt the application data   |
   |                                |                                | and to check message           |
   |                                |                                | integrity. If this occurs      |
   |                                |                                | frequently on a server, an     |
   |                                |                                | active attack (such as the     |
   |                                |                                | "million question" attack) may |
   |                                |                                | be underway against the        |
   |                                |                                | server.                        |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_BAD_MAC_ALERT        | -12272                         | "SSL peer reports incorrect    |
   |                                |                                | Message Authentication Code."  |
   |                                |                                | The remote system has reported |
   |                                |                                | that it received a message     |
   |                                |                                | with a bad Message             |
   |                                |                                | Authentication Code from the   |
   |                                |                                | local system. This may         |
   |                                |                                | indicate that an attack on     |
   |                                |                                | that server is underway.       |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_BAD_CERT_ALERT       | -12271                         | "SSL peer cannot verify your   |
   |                                |                                | certificate."                  |
   |                                |                                |                                |
   |                                |                                | The remote system has received |
   |                                |                                | a certificate from the local   |
   |                                |                                | system, and has rejected it    |
   |                                |                                | for some reason.               |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_REVOKED_CERT_ALERT   | -12270                         | "SSL peer rejected your        |
   |                                |                                | certificate as revoked."       |
   |                                |                                |                                |
   |                                |                                | The remote system has received |
   |                                |                                | a certificate from the local   |
   |                                |                                | system, and has determined     |
   |                                |                                | that the certificate has been  |
   |                                |                                | revoked.                       |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_EXPIRED_CERT_ALERT   | -12269                         | "SSL peer rejected your        |
   |                                |                                | certificate as expired."       |
   |                                |                                |                                |
   |                                |                                | The remote system has received |
   |                                |                                | a certificate from the local   |
   |                                |                                | system, and has determined     |
   |                                |                                | that the certificate has       |
   |                                |                                | expired.                       |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_SSL_DISABLED         | -12268                         | "Cannot connect: SSL is        |
   |                                |                                | disabled."                     |
   |                                |                                |                                |
   |                                |                                | The local socket is configured |
   |                                |                                | in such a way that it cannot   |
   |                                |                                | use any of the SSL cipher      |
   |                                |                                | suites. Possible causes        |
   |                                |                                | include: (a) both SSL2 and     |
   |                                |                                | SSL3 are disabled, (b) All the |
   |                                |                                | individual SSL cipher suites   |
   |                                |                                | are disabled, or (c) the       |
   |                                |                                | socket is configured to        |
   |                                |                                | handshake as a server, but the |
   |                                |                                | certificate associated with    |
   |                                |                                | that socket is inappropriate   |
   |                                |                                | for the Key Exchange Algorithm |
   |                                |                                | selected.                      |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_FORTEZZA_PQG         | -12267                         | "Cannot connect: SSL peer is   |
   |                                |                                | in another FORTEZZA domain."   |
   |                                |                                |                                |
   |                                |                                | The local system and the       |
   |                                |                                | remote system are in different |
   |                                |                                | FORTEZZA domains. They must be |
   |                                |                                | in the same domain to          |
   |                                |                                | communicate.                   |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_UNKNOWN_CIPHER_SUITE | -12266                         | "An unknown SSL cipher suite   |
   |                                |                                | has been requested."           |
   |                                |                                |                                |
   |                                |                                | The application has attempted  |
   |                                |                                | to configure SSL to use an     |
   |                                |                                | unknown cipher suite.          |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_NO_CIPHERS_SUPPORTED | -12265                         | "No cipher suites are present  |
   |                                |                                | and enabled in this program."  |
   |                                |                                |                                |
   |                                |                                | Possible causes: (a) all       |
   |                                |                                | cipher suites have been        |
   |                                |                                | configured to be disabled, (b) |
   |                                |                                | the only cipher suites that    |
   |                                |                                | are configured to be enabled   |
   |                                |                                | are those that are disallowed  |
   |                                |                                | by cipher export policy, (c)   |
   |                                |                                | the socket is configured to    |
   |                                |                                | handshake as a server, but the |
   |                                |                                | certificate associated with    |
   |                                |                                | that socket is inappropriate   |
   |                                |                                | for the Key Exchange Algorithm |
   |                                |                                | selected.                      |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_BAD_BLOCK_PADDING    | -12264                         | "SSL received a record with    |
   |                                |                                | bad block padding."            |
   |                                |                                |                                |
   |                                |                                | SSL was using a Block cipher,  |
   |                                |                                | and the last block in an SSL   |
   |                                |                                | record had incorrect padding   |
   |                                |                                | information in it. This        |
   |                                |                                | usually indicates that the     |
   |                                |                                | client and server have failed  |
   |                                |                                | to come to agreement on the    |
   |                                |                                | set of keys used to encrypt    |
   |                                |                                | the application data and to    |
   |                                |                                | check message integrity. If    |
   |                                |                                | this occurs frequently on a    |
   |                                |                                | server, an active attack (such |
   |                                |                                | as the "million question"      |
   |                                |                                | attack) may be underway        |
   |                                |                                | against the server.            |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_RX_RECORD_TOO_LONG   | -12263                         | "SSL received a record that    |
   |                                |                                | exceeded the maximum           |
   |                                |                                | permissible length."           |
   |                                |                                |                                |
   |                                |                                | This generally indicates that  |
   |                                |                                | the remote peer system has a   |
   |                                |                                | flawed implementation of SSL,  |
   |                                |                                | and is violating the SSL       |
   |                                |                                | specification.                 |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_TX_RECORD_TOO_LONG   | -12262                         | "SSL attempted to send a       |
   |                                |                                | record that exceeded the       |
   |                                |                                | maximum permissible length."   |
   |                                |                                |                                |
   |                                |                                | This error should never occur. |
   |                                |                                | If it does, it indicates a     |
   |                                |                                | flaw in the NSS SSL library.   |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_CLOSE_NOTIFY_ALERT   | -12230                         | "SSL peer has closed this      |
   |                                |                                | connection."                   |
   |                                |                                |                                |
   |                                |                                | The local socket received an   |
   |                                |                                | SSL3 alert record from the     |
   |                                |                                | remote peer, reporting that    |
   |                                |                                | the remote peer has chosen to  |
   |                                |                                | end the connection. The        |
   |                                |                                | receipt of this alert is an    |
   |                                |                                | error only if it occurs while  |
   |                                |                                | a handshake is in progress.    |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERR                        | -12210                         | "SSL Server attempted to use   |
   | OR_PUB_KEY_SIZE_LIMIT_EXCEEDED |                                | domestic-grade public key with |
   |                                |                                | export cipher suite."          |
   |                                |                                |                                |
   |                                |                                | On a client socket, this error |
   |                                |                                | reports that the remote server |
   |                                |                                | has failed to perform an "SSL  |
   |                                |                                | Step down" for an export       |
   |                                |                                | cipher. It has sent a          |
   |                                |                                | certificate bearing a          |
   |                                |                                | domestic-grade public key, but |
   |                                |                                | has not sent a                 |
   |                                |                                | ServerKeyExchange message      |
   |                                |                                | containing an export-grade     |
   |                                |                                | public key for the key         |
   |                                |                                | exchange algorithm. Such a     |
   |                                |                                | connection cannot be permitted |
   |                                |                                | without violating U.S. export  |
   |                                |                                | policies. On a server socket,  |
   |                                |                                | this indicates a failure of    |
   |                                |                                | the local library.             |
   +--------------------------------+--------------------------------+--------------------------------+
   | S                              | -12206                         | "Server has no key for the     |
   | SL_ERROR_NO_SERVER_KEY_FOR_ALG |                                | attempted key exchange         |
   |                                |                                | algorithm."                    |
   |                                |                                |                                |
   |                                |                                | An SSL client has requested an |
   |                                |                                | SSL cipher suite that uses a   |
   |                                |                                | Key Exchange Algorithm for     |
   |                                |                                | which the local server has no  |
   |                                |                                | appropriate public key. This   |
   |                                |                                | indicates a configuration      |
   |                                |                                | error on the local server.     |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL                            | -12205                         | "PKCS #11 token was inserted   |
   | _ERROR_TOKEN_INSERTION_REMOVAL |                                | or removed while operation was |
   |                                |                                | in progress."                  |
   |                                |                                |                                |
   |                                |                                | A cryptographic operation      |
   |                                |                                | required to complete the       |
   |                                |                                | handshake failed because the   |
   |                                |                                | token that was performing it   |
   |                                |                                | was removed while the          |
   |                                |                                | handshake was underway.        |
   |                                |                                | Another token may also have    |
   |                                |                                | been inserted into the same    |
   |                                |                                | slot.                          |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_TOKEN_SLOT_NOT_FOUND | -12204                         | "No PKCS#11 token could be     |
   |                                |                                | found to do a required         |
   |                                |                                | operation."                    |
   |                                |                                |                                |
   |                                |                                | A cryptographic operation      |
   |                                |                                | required a PKCS#11 token with  |
   |                                |                                | specific abilities, and no     |
   |                                |                                | token could be found in any    |
   |                                |                                | slot, including the "soft      |
   |                                |                                | token" in the internal virtual |
   |                                |                                | slot, that could do the job.   |
   |                                |                                | May indicate a server          |
   |                                |                                | configuration error, such as   |
   |                                |                                | having a certificate that is   |
   |                                |                                | inappropriate for the Key      |
   |                                |                                | Exchange Algorithm selected.   |
   +--------------------------------+--------------------------------+--------------------------------+
   | SS                             | -12203                         | "Cannot communicate securely   |
   | L_ERROR_NO_COMPRESSION_OVERLAP |                                | with peer: no common           |
   |                                |                                | compression algorithm(s)."     |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL                            | -12202                         | "Cannot initiate another SSL   |
   | _ERROR_HANDSHAKE_NOT_COMPLETED |                                | handshake until current        |
   |                                |                                | handshake is complete."        |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_                           | -12201                         | "Received incorrect handshakes |
   | ERROR_BAD_HANDSHAKE_HASH_VALUE |                                | hash values from peer."        |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_CERT_KEA_MISMATCH    | -12200                         | "The certificate provided      |
   |                                |                                | cannot be used with the        |
   |                                |                                | selected key exchange          |
   |                                |                                | algorithm."                    |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_                           | -12199                         | "No certificate authority is   |
   | ERROR_NO_TRUSTED_SSL_CLIENT_CA |                                | trusted for SSL client         |
   |                                |                                | authentication."               |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_SESSION_NOT_FOUND    | -12198                         | "Client's SSL session ID not   |
   |                                |                                | found in server's session      |
   |                                |                                | cache."                        |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERR                        | -12185                         | "SSL server cache not          |
   | OR_SERVER_CACHE_NOT_CONFIGURED |                                | configured and not disabled    |
   |                                |                                | for this socket."              |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_E                          | -12176                         | "Renegotiation is not allowed  |
   | RROR_RENEGOTIATION_NOT_ALLOWED |                                | on this SSL socket."           |
   +--------------------------------+--------------------------------+--------------------------------+
   | **Received a malformed (too    |                                |                                |
   | long or short or invalid       |                                |                                |
   | content) SSL handshake: **     |                                |                                |
   |                                |                                |                                |
   | All the error codes in the     |                                |                                |
   | following block indicate that  |                                |                                |
   | the local socket received an   |                                |                                |
   | improperly formatted SSL3      |                                |                                |
   | handshake message from the     |                                |                                |
   | remote peer. This probably     |                                |                                |
   | indicates a flaw in the remote |                                |                                |
   | peer's implementation.         |                                |                                |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ER                         | -12261                         | "SSL received a malformed      |
   | ROR_RX_MALFORMED_HELLO_REQUEST |                                | Hello Request handshake        |
   |                                |                                | message."                      |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_E                          | -12260                         | "SSL received a malformed      |
   | RROR_RX_MALFORMED_CLIENT_HELLO |                                | Client Hello handshake         |
   |                                |                                | message."                      |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_E                          | -12259                         | "SSL received a malformed      |
   | RROR_RX_MALFORMED_SERVER_HELLO |                                | Server Hello handshake         |
   |                                |                                | message."                      |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_                           | -12258                         | "SSL received a malformed      |
   | ERROR_RX_MALFORMED_CERTIFICATE |                                | Certificate handshake          |
   |                                |                                | message."                      |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR                      | -12257                         | "SSL received a malformed      |
   | _RX_MALFORMED_SERVER_KEY_EXCH  |                                | Server Key Exchange handshake  |
   |                                |                                | message."                      |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_E                          | -12256                         | "SSL received a malformed      |
   | RROR_RX_MALFORMED_CERT_REQUEST |                                | Certificate Request handshake  |
   |                                |                                | message."                      |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL                            | -12255                         | "SSL received a malformed      |
   | _ERROR_RX_MALFORMED_HELLO_DONE |                                | Server Hello Done handshake    |
   |                                |                                | message."                      |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_                           | -12254                         | "SSL received a malformed      |
   | ERROR_RX_MALFORMED_CERT_VERIFY |                                | Certificate Verify handshake   |
   |                                |                                | message."                      |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR                      | -12253                         | "SSL received a malformed      |
   | _RX_MALFORMED_CLIENT_KEY_EXCH  |                                | Client Key Exchange handshake  |
   |                                |                                | message."                      |
   +--------------------------------+--------------------------------+--------------------------------+
   | S                              | -12252                         | "SSL received a malformed      |
   | SL_ERROR_RX_MALFORMED_FINISHED |                                | Finished handshake message."   |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_R                    | -12178                         | "SSL received a malformed New  |
   | X_MALFORMED_NEW_SESSION_TICKET |                                | Session Ticket handshake       |
   |                                |                                | message."                      |
   +--------------------------------+--------------------------------+--------------------------------+
   | **Received a malformed (too    |                                |                                |
   | long or short) SSL record:**   |                                |                                |
   |                                |                                |                                |
   | All the error codes in the     |                                |                                |
   | following block indicate that  |                                |                                |
   | the local socket received an   |                                |                                |
   | improperly formatted SSL3      |                                |                                |
   | record from the remote peer.   |                                |                                |
   | This probably indicates a flaw |                                |                                |
   | in the remote peer's           |                                |                                |
   | implementation.                |                                |                                |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ER                         | -12251                         | "SSL received a malformed      |
   | ROR_RX_MALFORMED_CHANGE_CIPHER |                                | Change Cipher Spec record."    |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_RX_MALFORMED_ALERT   | -12250                         | "SSL received a malformed      |
   |                                |                                | Alert record."                 |
   +--------------------------------+--------------------------------+--------------------------------+
   | SS                             | -12249                         | "SSL received a malformed      |
   | L_ERROR_RX_MALFORMED_HANDSHAKE |                                | Handshake record."             |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_                     | -12248                         | "SSL received a malformed      |
   | RX_MALFORMED_APPLICATION_DATA  |                                | Application Data record."      |
   +--------------------------------+--------------------------------+--------------------------------+
   | **Received an SSL handshake    |                                |                                |
   | that was inappropriate for the |                                |                                |
   | current state:**               |                                |                                |
   |                                |                                |                                |
   | All the error codes in the     |                                |                                |
   | following block indicate that  |                                |                                |
   | the local socket received an   |                                |                                |
   | SSL3 handshake message from    |                                |                                |
   | the remote peer at a time when |                                |                                |
   | it was inappropriate for the   |                                |                                |
   | peer to have sent this         |                                |                                |
   | message. For example, a server |                                |                                |
   | received a message from        |                                |                                |
   | another server. This probably  |                                |                                |
   | indicates a flaw in the remote |                                |                                |
   | peer's implementation.         |                                |                                |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERR                        | -12247                         | "SSL received an unexpected    |
   | OR_RX_UNEXPECTED_HELLO_REQUEST |                                | Hello Request handshake        |
   |                                |                                | message."                      |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ER                         | -12246                         | "SSL received an unexpected    |
   | ROR_RX_UNEXPECTED_CLIENT_HELLO |                                | Client Hello handshake         |
   |                                |                                | message."                      |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ER                         | -12245                         | "SSL received an unexpected    |
   | ROR_RX_UNEXPECTED_SERVER_HELLO |                                | Server Hello handshake         |
   |                                |                                | message."                      |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_E                          | -12244                         | "SSL received an unexpected    |
   | RROR_RX_UNEXPECTED_CERTIFICATE |                                | Certificate handshake          |
   |                                |                                | message."                      |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_                     | -12243                         | "SSL received an unexpected    |
   | RX_UNEXPECTED_SERVER_KEY_EXCH  |                                | Server Key Exchange handshake  |
   |                                |                                | message."                      |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ER                         | -12242                         | "SSL received an unexpected    |
   | ROR_RX_UNEXPECTED_CERT_REQUEST |                                | Certificate Request handshake  |
   |                                |                                | message."                      |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_                           | -12241                         | "SSL received an unexpected    |
   | ERROR_RX_UNEXPECTED_HELLO_DONE |                                | Server Hello Done handshake    |
   |                                |                                | message."                      |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_E                          | -12240                         | "SSL received an unexpected    |
   | RROR_RX_UNEXPECTED_CERT_VERIFY |                                | Certificate Verify handshake   |
   |                                |                                | message."                      |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_                     | -12239                         | "SSL received an unexpected    |
   | RX_UNEXPECTED_CLIENT_KEY_EXCH  |                                | Client Key Exchange handshake  |
   |                                |                                | message."                      |
   +--------------------------------+--------------------------------+--------------------------------+
   | SS                             | -12238                         | "SSL received an unexpected    |
   | L_ERROR_RX_UNEXPECTED_FINISHED |                                | Finished handshake message."   |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_RX                   | -12179                         | "SSL received an unexpected    |
   | _UNEXPECTED_NEW_SESSION_TICKET |                                | New Session Ticket handshake   |
   |                                |                                | message."                      |
   +--------------------------------+--------------------------------+--------------------------------+
   | **Received an SSL record that  |                                |                                |
   | was inappropriate for the      |                                |                                |
   | current state:**               |                                |                                |
   |                                |                                |                                |
   | All the error codes in the     |                                |                                |
   | following block indicate that  |                                |                                |
   | the local socket received an   |                                |                                |
   | SSL3 record from the remote    |                                |                                |
   | peer at a time when it was     |                                |                                |
   | inappropriate for the peer to  |                                |                                |
   | have sent this message. This   |                                |                                |
   | probably indicates a flaw in   |                                |                                |
   | the remote peer's              |                                |                                |
   | implementation.                |                                |                                |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERR                        | -12237                         | "SSL received an unexpected    |
   | OR_RX_UNEXPECTED_CHANGE_CIPHER |                                | Change Cipher Spec record."    |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_RX_UNEXPECTED_ALERT  | -12236                         | "SSL received an unexpected    |
   |                                |                                | Alert record."                 |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL                            | -12235                         | "SSL received an unexpected    |
   | _ERROR_RX_UNEXPECTED_HANDSHAKE |                                | Handshake record."             |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_                     | -12234                         | "SSL received an unexpected    |
   | RX_UNEXPECTED_APPLICATION_DATA |                                | Application Data record."      |
   +--------------------------------+--------------------------------+--------------------------------+
   | **Received record/message with |                                |                                |
   | unknown discriminant:**        |                                |                                |
   |                                |                                |                                |
   | All the error codes in the     |                                |                                |
   | following block indicate that  |                                |                                |
   | the local socket received an   |                                |                                |
   | SSL3 record or handshake       |                                |                                |
   | message from the remote peer   |                                |                                |
   | that it was unable to          |                                |                                |
   | interpret because the byte     |                                |                                |
   | that identifies the type of    |                                |                                |
   | record or message contained an |                                |                                |
   | unrecognized value. This       |                                |                                |
   | probably indicates a flaw in   |                                |                                |
   | the remote peer's              |                                |                                |
   | implementation.                |                                |                                |
   +--------------------------------+--------------------------------+--------------------------------+
   | SS                             | -12233                         | "SSL received a record with an |
   | L_ERROR_RX_UNKNOWN_RECORD_TYPE |                                | unknown content type."         |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_RX_UNKNOWN_HANDSHAKE | -12232                         | "SSL received a handshake      |
   |                                |                                | message with an unknown        |
   |                                |                                | message type."                 |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_RX_UNKNOWN_ALERT     | -12231                         | "SSL received an alert record  |
   |                                |                                | with an unknown alert          |
   |                                |                                | description."                  |
   +--------------------------------+--------------------------------+--------------------------------+
   | **Received an alert report:**  |                                |                                |
   |                                |                                |                                |
   | | All the error codes in the   |                                |                                |
   |   following block indicate     |                                |                                |
   |   that the local socket        |                                |                                |
   |   received an SSL3 or TLS      |                                |                                |
   |   alert record from the remote |                                |                                |
   |   peer, reporting some issue   |                                |                                |
   |   that it had with an SSL      |                                |                                |
   |   record or handshake message  |                                |                                |
   |   it received. (Some \_Alert   |                                |                                |
   |   codes are listed in other    |                                |                                |
   |   blocks.)                     |                                |                                |
   | |                              |                                |                                |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ER                         | -12229                         | "SSL peer was not expecting a  |
   | ROR_HANDSHAKE_UNEXPECTED_ALERT |                                | handshake message it           |
   |                                |                                | received."                     |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERR                        | -12228                         | "SSL peer was unable to        |
   | OR_DECOMPRESSION_FAILURE_ALERT |                                | successfully decompress an SSL |
   |                                |                                | record it received."           |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL                            | -12227                         | "SSL peer was unable to        |
   | _ERROR_HANDSHAKE_FAILURE_ALERT |                                | negotiate an acceptable set of |
   |                                |                                | security parameters."          |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL                            | -12226                         | "SSL peer rejected a handshake |
   | _ERROR_ILLEGAL_PARAMETER_ALERT |                                | message for unacceptable       |
   |                                |                                | content."                      |
   +--------------------------------+--------------------------------+--------------------------------+
   | SS                             | -12225                         | "SSL peer does not support     |
   | L_ERROR_UNSUPPORTED_CERT_ALERT |                                | certificates of the type it    |
   |                                |                                | received."                     |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_E                          | -12224                         | "SSL peer had some unspecified |
   | RROR_CERTIFICATE_UNKNOWN_ALERT |                                | issue with the certificate it  |
   |                                |                                | received."                     |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL                            | -12197                         | "Peer was unable to decrypt an |
   | _ERROR_DECRYPTION_FAILED_ALERT |                                | SSL record it received."       |
   +--------------------------------+--------------------------------+--------------------------------+
   | S                              | -12196                         | "Peer received an SSL record   |
   | SL_ERROR_RECORD_OVERFLOW_ALERT |                                | that was longer than is        |
   |                                |                                | permitted."                    |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_UNKNOWN_CA_ALERT     | -12195                         | "Peer does not recognize and   |
   |                                |                                | trust the CA that issued your  |
   |                                |                                | certificate."                  |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_ACCESS_DENIED_ALERT  | -12194                         | "Peer received a valid         |
   |                                |                                | certificate, but access was    |
   |                                |                                | denied."                       |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_DECODE_ERROR_ALERT   | -12193                         | "Peer could not decode an SSL  |
   |                                |                                | handshake message."            |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_DECRYPT_ERROR_ALERT  | -12192                         | "Peer reports failure of       |
   |                                |                                | signature verification or key  |
   |                                |                                | exchange."                     |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_                           | -12191                         | "Peer reports negotiation not  |
   | ERROR_EXPORT_RESTRICTION_ALERT |                                | in compliance with export      |
   |                                |                                | regulations."                  |
   +--------------------------------+--------------------------------+--------------------------------+
   | SS                             | -12190                         | "Peer reports incompatible or  |
   | L_ERROR_PROTOCOL_VERSION_ALERT |                                | unsupported protocol version." |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERR                        | -12189                         | "Server requires ciphers more  |
   | OR_INSUFFICIENT_SECURITY_ALERT |                                | secure than those supported by |
   |                                |                                | client."                       |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_INTERNAL_ERROR_ALERT | -12188                         | "Peer reports it experienced   |
   |                                |                                | an internal error."            |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_USER_CANCELED_ALERT  | -12187                         | "Peer user canceled            |
   |                                |                                | handshake."                    |
   +--------------------------------+--------------------------------+--------------------------------+
   | SS                             | -12186                         | "Peer does not permit          |
   | L_ERROR_NO_RENEGOTIATION_ALERT |                                | renegotiation of SSL security  |
   |                                |                                | parameters."                   |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERR                        | -12184                         | "SSL peer does not support     |
   | OR_UNSUPPORTED_EXTENSION_ALERT |                                | requested TLS hello            |
   |                                |                                | extension."                    |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_                     | -12183                         | "SSL peer could not obtain     |
   | CERTIFICATE_UNOBTAINABLE_ALERT |                                | your certificate from the      |
   |                                |                                | supplied URL."                 |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL                            | -12182                         | "SSL peer has no certificate   |
   | _ERROR_UNRECOGNIZED_NAME_ALERT |                                | for the requested DNS name."   |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_                     | -12181                         | "SSL peer was unable to get an |
   | BAD_CERT_STATUS_RESPONSE_ALERT |                                | OCSP response for its          |
   |                                |                                | certificate."                  |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_E                          | -12180                         | "SSL peer reported bad         |
   | RROR_BAD_CERT_HASH_VALUE_ALERT |                                | certificate hash value."       |
   +--------------------------------+--------------------------------+--------------------------------+
   | **Unspecified errors that      |                                |                                |
   | occurred while attempting some |                                |                                |
   | operation:**                   |                                |                                |
   |                                |                                |                                |
   | All the error codes in the     |                                |                                |
   | following block describe the   |                                |                                |
   | operation that was being       |                                |                                |
   | attempted at the time of the   |                                |                                |
   | unspecified failure. These     |                                |                                |
   | failures may be caused by the  |                                |                                |
   | system running out of memory,  |                                |                                |
   | or errors returned by PKCS#11  |                                |                                |
   | routines that did not provide  |                                |                                |
   | meaningful error codes of      |                                |                                |
   | their own. These should rarely |                                |                                |
   | be seen. (Certain of these     |                                |                                |
   | error codes have more specific |                                |                                |
   | meanings, as described.)       |                                |                                |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL                            | -12223                         | "SSL experienced a failure of  |
   | _ERROR_GENERATE_RANDOM_FAILURE |                                | its random number generator."  |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_SIGN_HASHES_FAILURE  | -12222                         | "Unable to digitally sign data |
   |                                |                                | required to verify your        |
   |                                |                                | certificate."                  |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ER                         | -12221                         | "SSL was unable to extract the |
   | ROR_EXTRACT_PUBLIC_KEY_FAILURE |                                | public key from the peer's     |
   |                                |                                | certificate."                  |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERR                        | -12220                         | "Unspecified failure while     |
   | OR_SERVER_KEY_EXCHANGE_FAILURE |                                | processing SSL Server Key      |
   |                                |                                | Exchange handshake."           |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERR                        | -12219                         | "Unspecified failure while     |
   | OR_CLIENT_KEY_EXCHANGE_FAILURE |                                | processing SSL Client Key      |
   |                                |                                | Exchange handshake."           |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_ENCRYPTION_FAILURE   | -12218                         | "Bulk data encryption          |
   |                                |                                | algorithm failed in selected   |
   |                                |                                | cipher suite."                 |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_DECRYPTION_FAILURE   | -12217                         | "Bulk data decryption          |
   |                                |                                | algorithm failed in selected   |
   |                                |                                | cipher suite."                 |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_SOCKET_WRITE_FAILURE | -12216                         | "Attempt to write encrypted    |
   |                                |                                | data to underlying socket      |
   |                                |                                | failed."                       |
   |                                |                                |                                |
   |                                |                                | After the data to be sent was  |
   |                                |                                | encrypted, the attempt to send |
   |                                |                                | it out the socket failed.      |
   |                                |                                | Likely causes include that the |
   |                                |                                | peer has closed the            |
   |                                |                                | connection.                    |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_MD5_DIGEST_FAILURE   | -12215                         | "MD5 digest function failed."  |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_SHA_DIGEST_FAILURE   | -12214                         | "SHA-1 digest function         |
   |                                |                                | failed."                       |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL                            | -12213                         | "Message Authentication Code   |
   | _ERROR_MAC_COMPUTATION_FAILURE |                                | computation failed."           |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL                            | -12212                         | "Failure to create Symmetric   |
   | _ERROR_SYM_KEY_CONTEXT_FAILURE |                                | Key context."                  |
   +--------------------------------+--------------------------------+--------------------------------+
   | SS                             | -12211                         | "Failure to unwrap the         |
   | L_ERROR_SYM_KEY_UNWRAP_FAILURE |                                | Symmetric key in Client Key    |
   |                                |                                | Exchange message."             |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_ERROR_IV_PARAM_FAILURE     | -12209                         | "PKCS11 code failed to         |
   |                                |                                | translate an IV into a param." |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL_E                          | -12208                         | "Failed to initialize the      |
   | RROR_INIT_CIPHER_SUITE_FAILURE |                                | selected cipher suite."        |
   +--------------------------------+--------------------------------+--------------------------------+
   | SSL                            | -12207                         | "Failed to generate session    |
   | _ERROR_SESSION_KEY_GEN_FAILURE |                                | keys for SSL session."         |
   |                                |                                |                                |
   |                                |                                | On a client socket, indicates  |
   |                                |                                | a failure of the PKCS11 key    |
   |                                |                                | generation function. On a      |
   |                                |                                | server socket, indicates a     |
   |                                |                                | failure of one of the          |
   |                                |                                | following: (a) to unwrap the   |
   |                                |                                | pre-master secret from the     |
   |                                |                                | ClientKeyExchange message, (b) |
   |                                |                                | to derive the master secret    |
   |                                |                                | from the premaster secret, (c) |
   |                                |                                | to derive the MAC secrets,     |
   |                                |                                | cryptographic keys, and        |
   |                                |                                | initialization vectors from    |
   |                                |                                | the master secret. If          |
   |                                |                                | encountered repeatedly on a    |
   |                                |                                | server socket, this can        |
   |                                |                                | indicate that the server is    |
   |                                |                                | actively under a "million      |
   |                                |                                | question" attack.              |
   +--------------------------------+--------------------------------+--------------------------------+
   | S                              | -12177                         | "SSL received a compressed     |
   | SL_ERROR_DECOMPRESSION_FAILURE |                                | record that could not be       |
   |                                |                                | decompressed."                 |
   +--------------------------------+--------------------------------+--------------------------------+

.. _sec_error_codes:

`SEC Error Codes <#sec_error_codes>`__
--------------------------------------

.. container::

   **Table 8.2 Security error codes defined in secerr.h**

   +--------------------------------+--------------------------------+--------------------------------+
   | **Constant**                   | **Value**                      | **Description**                |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_IO                   | -8192                          | An I/O error occurred during   |
   |                                |                                | authentication; or             |
   |                                |                                | an error occurred during       |
   |                                |                                | crypto operation (other than   |
   |                                |                                | signature verification).       |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_LIBRARY_FAILURE      | -8191                          | Security library failure.      |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_BAD_DATA             | -8190                          | Security library: received bad |
   |                                |                                | data.                          |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_OUTPUT_LEN           | -8189                          | Security library: output       |
   |                                |                                | length error.                  |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_INPUT_LEN            | -8188                          | Security library: input length |
   |                                |                                | error.                         |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_INVALID_ARGS         | -8187                          | Security library: invalid      |
   |                                |                                | arguments.                     |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_INVALID_ALGORITHM    | -8186                          | Security library: invalid      |
   |                                |                                | algorithm.                     |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_INVALID_AVA          | -8185                          | Security library: invalid AVA. |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_INVALID_TIME         | -8184                          | Security library: invalid      |
   |                                |                                | time.                          |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_BAD_DER              | -8183                          | Security library: improperly   |
   |                                |                                | formatted DER-encoded message. |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_BAD_SIGNATURE        | -8182                          | Peer's certificate has an      |
   |                                |                                | invalid signature.             |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_EXPIRED_CERTIFICATE  | -8181                          | Peer's certificate has         |
   |                                |                                | expired.                       |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_REVOKED_CERTIFICATE  | -8180                          | Peer's certificate has been    |
   |                                |                                | revoked.                       |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_UNKNOWN_ISSUER       | -8179                          | Peer's certificate issuer is   |
   |                                |                                | not recognized.                |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_BAD_KEY              | -8178                          | Peer's public key is invalid   |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_BAD_PASSWORD         | -8177                          | The password entered is        |
   |                                |                                | incorrect.                     |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_RETRY_PASSWORD       | -8176                          | New password entered           |
   |                                |                                | incorrectly.                   |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_NO_NODELOCK          | -8175                          | Security library: no nodelock. |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_BAD_DATABASE         | -8174                          | Security library: bad          |
   |                                |                                | database.                      |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_NO_MEMORY            | -8173                          | Security library: memory       |
   |                                |                                | allocation failure.            |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_UNTRUSTED_ISSUER     | -8172                          | Peer's certificate issuer has  |
   |                                |                                | been marked as not trusted by  |
   |                                |                                | the user.                      |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_UNTRUSTED_CERT       | -8171                          | Peer's certificate has been    |
   |                                |                                | marked as not trusted by the   |
   |                                |                                | user.                          |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_DUPLICATE_CERT       | -8170                          | Certificate already exists in  |
   |                                |                                | your database.                 |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_DUPLICATE_CERT_NAME  | -8169                          | Downloaded certificate's name  |
   |                                |                                | duplicates one already in your |
   |                                |                                | database.                      |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_ADDING_CERT          | -8168                          | Error adding certificate to    |
   |                                |                                | database.                      |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_FILING_KEY           | -8167                          | Error refiling the key for     |
   |                                |                                | this certificate.              |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_NO_KEY               | -8166                          | The private key for this       |
   |                                |                                | certificate cannot be found in |
   |                                |                                | key database.                  |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_CERT_VALID           | -8165                          | This certificate is valid.     |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_CERT_NOT_VALID       | -8164                          | This certificate is not valid. |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_CERT_NO_RESPONSE     | -8163                          | Certificate library: no        |
   |                                |                                | response.                      |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ER                         | -8162                          | The certificate issuer's       |
   | ROR_EXPIRED_ISSUER_CERTIFICATE |                                | certificate has expired.       |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_CRL_EXPIRED          | -8161                          | The CRL for the certificate's  |
   |                                |                                | issuer has expired.            |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_CRL_BAD_SIGNATURE    | -8160                          | The CRL for the certificate's  |
   |                                |                                | issuer has an invalid          |
   |                                |                                | signature.                     |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_CRL_INVALID          | -8159                          | New CRL has an invalid format. |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC                            | -8158                          | Certificate extension value is |
   | _ERROR_EXTENSION_VALUE_INVALID |                                | invalid.                       |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_EXTENSION_NOT_FOUND  | -8157                          | Certificate extension not      |
   |                                |                                | found.                         |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_CA_CERT_INVALID      | -8156                          | Issuer certificate is invalid. |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERR                        | -8155                          | Certificate path length        |
   | OR_PATH_LEN_CONSTRAINT_INVALID |                                | constraint is invalid.         |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_CERT_USAGES_INVALID  | -8154                          | Certificate usages field is    |
   |                                |                                | invalid.                       |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_INTERNAL_ONLY              | -8153                          | Internal-only module.          |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_INVALID_KEY          | -8152                          | The key does not support the   |
   |                                |                                | requested operation.           |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ER                         | -8151                          | Certificate contains unknown   |
   | ROR_UNKNOWN_CRITICAL_EXTENSION |                                | critical extension.            |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_OLD_CRL              | -8150                          | New CRL is not later than the  |
   |                                |                                | current one.                   |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_NO_EMAIL_CERT        | -8149                          | Not encrypted or signed: you   |
   |                                |                                | do not yet have an email       |
   |                                |                                | certificate.                   |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_                           | -8148                          | Not encrypted: you do not have |
   | ERROR_NO_RECIPIENT_CERTS_QUERY |                                | certificates for each of the   |
   |                                |                                | recipients.                    |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_NOT_A_RECIPIENT      | -8147                          | Cannot decrypt: you are not a  |
   |                                |                                | recipient, or matching         |
   |                                |                                | certificate and private key    |
   |                                |                                | not found.                     |
   +--------------------------------+--------------------------------+--------------------------------+
   | S                              | -8146                          | Cannot decrypt: key encryption |
   | EC_ERROR_PKCS7_KEYALG_MISMATCH |                                | algorithm does not match your  |
   |                                |                                | certificate.                   |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_PKCS7_BAD_SIGNATURE  | -8145                          | Signature verification failed: |
   |                                |                                | no signer found, too many      |
   |                                |                                | signers found, \\              |
   |                                |                                | or improper or corrupted data. |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_UNSUPPORTED_KEYALG   | -8144                          | Unsupported or unknown key     |
   |                                |                                | algorithm.                     |
   +--------------------------------+--------------------------------+--------------------------------+
   | S                              | -8143                          | Cannot decrypt: encrypted      |
   | EC_ERROR_DECRYPTION_DISALLOWED |                                | using a disallowed algorithm   |
   |                                |                                | or key size.                   |
   +--------------------------------+--------------------------------+--------------------------------+
   | XP_SEC_FORTEZZA_BAD_CARD       | -8142                          | FORTEZZA card has not been     |
   |                                |                                | properly initialized.          |
   +--------------------------------+--------------------------------+--------------------------------+
   | XP_SEC_FORTEZZA_NO_CARD        | -8141                          | No FORTEZZA cards found.       |
   +--------------------------------+--------------------------------+--------------------------------+
   | XP_SEC_FORTEZZA_NONE_SELECTED  | -8140                          | No FORTEZZA card selected.     |
   +--------------------------------+--------------------------------+--------------------------------+
   | XP_SEC_FORTEZZA_MORE_INFO      | -8139                          | Please select a personality to |
   |                                |                                | get more info on.              |
   +--------------------------------+--------------------------------+--------------------------------+
   | XP                             | -8138                          | Personality not found          |
   | _SEC_FORTEZZA_PERSON_NOT_FOUND |                                |                                |
   +--------------------------------+--------------------------------+--------------------------------+
   | XP_SEC_FORTEZZA_NO_MORE_INFO   | -8137                          | No more information on that    |
   |                                |                                | personality.                   |
   +--------------------------------+--------------------------------+--------------------------------+
   | XP_SEC_FORTEZZA_BAD_PIN        | -8136                          | Invalid PIN.                   |
   +--------------------------------+--------------------------------+--------------------------------+
   | XP_SEC_FORTEZZA_PERSON_ERROR   | -8135                          | Couldn't initialize FORTEZZA   |
   |                                |                                | personalities.                 |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_NO_KRL               | -8134                          | No KRL for this site's         |
   |                                |                                | certificate has been found.    |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_KRL_EXPIRED          | -8133                          | The KRL for this site's        |
   |                                |                                | certificate has expired.       |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_KRL_BAD_SIGNATURE    | -8132                          | The KRL for this site's        |
   |                                |                                | certificate has an invalid     |
   |                                |                                | signature.                     |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_REVOKED_KEY          | -8131                          | The key for this site's        |
   |                                |                                | certificate has been revoked.  |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_KRL_INVALID          | -8130                          | New KRL has an invalid format. |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_NEED_RANDOM          | -8129                          | Security library: need random  |
   |                                |                                | data.                          |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_NO_MODULE            | -8128                          | Security library: no security  |
   |                                |                                | module can perform the         |
   |                                |                                | requested operation.           |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_NO_TOKEN             | -8127                          | The security card or token     |
   |                                |                                | does not exist, needs to be    |
   |                                |                                | initialized, or has been       |
   |                                |                                | removed.                       |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_READ_ONLY            | -8126                          | Security library: read-only    |
   |                                |                                | database.                      |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_NO_SLOT_SELECTED     | -8125                          | No slot or token was selected. |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC                            | -8124                          | A certificate with the same    |
   | _ERROR_CERT_NICKNAME_COLLISION |                                | nickname already exists.       |
   +--------------------------------+--------------------------------+--------------------------------+
   | SE                             | -8123                          | A key with the same nickname   |
   | C_ERROR_KEY_NICKNAME_COLLISION |                                | already exists.                |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_SAFE_NOT_CREATED     | -8122                          | Error while creating safe      |
   |                                |                                | object.                        |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_BAGGAGE_NOT_CREATED  | -8121                          | Error while creating baggage   |
   |                                |                                | object.                        |
   +--------------------------------+--------------------------------+--------------------------------+
   | XP_JAVA_REMOVE_PRINCIPAL_ERROR | -8120                          | Couldn't remove the principal. |
   +--------------------------------+--------------------------------+--------------------------------+
   | XP_JAVA_DELETE_PRIVILEGE_ERROR | -8119                          | Couldn't delete the privilege  |
   +--------------------------------+--------------------------------+--------------------------------+
   | XP_JAVA_CERT_NOT_EXISTS_ERROR  | -8118                          | This principal doesn't have a  |
   |                                |                                | certificate.                   |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_BAD_EXPORT_ALGORITHM | -8117                          | Required algorithm is not      |
   |                                |                                | allowed.                       |
   +--------------------------------+--------------------------------+--------------------------------+
   | SE                             | -8116                          | Error attempting to export     |
   | C_ERROR_EXPORTING_CERTIFICATES |                                | certificates.                  |
   +--------------------------------+--------------------------------+--------------------------------+
   | SE                             | -8115                          | Error attempting to import     |
   | C_ERROR_IMPORTING_CERTIFICATES |                                | certificates.                  |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_PKCS12_DECODING_PFX  | -8114                          | Unable to import. Decoding     |
   |                                |                                | error. File not valid.         |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_PKCS12_INVALID_MAC   | -8113                          | Unable to import. Invalid MAC. |
   |                                |                                | Incorrect password or corrupt  |
   |                                |                                | file.                          |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_PK                   | -8112                          | Unable to import. MAC          |
   | CS12_UNSUPPORTED_MAC_ALGORITHM |                                | algorithm not supported.       |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_PKC                  | -8111                          | Unable to import. Only         |
   | S12_UNSUPPORTED_TRANSPORT_MODE |                                | password integrity and privacy |
   |                                |                                | modes supported.               |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR                      | -8110                          | Unable to import. File         |
   | _PKCS12_CORRUPT_PFX_STRUCTURE  |                                | structure is corrupt.          |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_PK                   | -8109                          | Unable to import. Encryption   |
   | CS12_UNSUPPORTED_PBE_ALGORITHM |                                | algorithm not supported.       |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ER                         | -8108                          | Unable to import. File version |
   | ROR_PKCS12_UNSUPPORTED_VERSION |                                | not supported.                 |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_PKC                  | -8107                          | Unable to import. Incorrect    |
   | S12_PRIVACY_PASSWORD_INCORRECT |                                | privacy password.              |
   +--------------------------------+--------------------------------+--------------------------------+
   | S                              | -8106                          | Unable to import. Same         |
   | EC_ERROR_PKCS12_CERT_COLLISION |                                | nickname already exists in     |
   |                                |                                | database.                      |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_USER_CANCELLED       | -8105                          | The user clicked cancel.       |
   +--------------------------------+--------------------------------+--------------------------------+
   | S                              | -8104                          | Not imported, already in       |
   | EC_ERROR_PKCS12_DUPLICATE_DATA |                                | database.                      |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_MESSAGE_SEND_ABORTED | -8103                          | Message not sent.              |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_INADEQUATE_KEY_USAGE | -8102                          | Certificate key usage          |
   |                                |                                | inadequate for attempted       |
   |                                |                                | operation.                     |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_INADEQUATE_CERT_TYPE | -8101                          | Certificate type not approved  |
   |                                |                                | for application.               |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_CERT_ADDR_MISMATCH   | -8100                          | Address in signing certificate |
   |                                |                                | does not match address in      |
   |                                |                                | message headers.               |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERR                        | -8099                          | Unable to import. Error        |
   | OR_PKCS12_UNABLE_TO_IMPORT_KEY |                                | attempting to import private   |
   |                                |                                | key.                           |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERR                        | -8098                          | Unable to import. Error        |
   | OR_PKCS12_IMPORTING_CERT_CHAIN |                                | attempting to import           |
   |                                |                                | certificate chain.             |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_PKCS12_U             | -8097                          | Unable to export. Unable to    |
   | NABLE_TO_LOCATE_OBJECT_BY_NAME |                                | locate certificate or key by   |
   |                                |                                | nickname.                      |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERRO                       | -8096                          | Unable to export. Private key  |
   | R_PKCS12_UNABLE_TO_EXPORT_KEY  |                                | could not be located and       |
   |                                |                                | exported.                      |
   +--------------------------------+--------------------------------+--------------------------------+
   | SE                             | -8095                          | Unable to export. Unable to    |
   | C_ERROR_PKCS12_UNABLE_TO_WRITE |                                | write the export file.         |
   +--------------------------------+--------------------------------+--------------------------------+
   | S                              | -8094                          | Unable to import. Unable to    |
   | EC_ERROR_PKCS12_UNABLE_TO_READ |                                | read the import file.          |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_PKCS1                | -8093                          | Unable to export. Key database |
   | 2_KEY_DATABASE_NOT_INITIALIZED |                                | corrupt or deleted.            |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_KEYGEN_FAIL          | -8092                          | Unable to generate             |
   |                                |                                | public-private key pair.       |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_INVALID_PASSWORD     | -8091                          | Password entered is invalid.   |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_RETRY_OLD_PASSWORD   | -8090                          | Old password entered           |
   |                                |                                | incorrectly.                   |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_BAD_NICKNAME         | -8089                          | Certificate nickname already   |
   |                                |                                | in use.                        |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_NOT_FORTEZZA_ISSUER  | -8088                          | Peer FORTEZZA chain has a      |
   |                                |                                | non-FORTEZZA Certificate.      |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_E                          | -8087                          | "A sensitive key cannot be     |
   | RROR_CANNOT_MOVE_SENSITIVE_KEY |                                | moved to the slot where it is  |
   |                                |                                | needed."                       |
   +--------------------------------+--------------------------------+--------------------------------+
   | SE                             | -8086                          | Invalid module name.           |
   | C_ERROR_JS_INVALID_MODULE_NAME |                                |                                |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_JS_INVALID_DLL       | -8085                          | Invalid module path/filename.  |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_JS_ADD_MOD_FAILURE   | -8084                          | Unable to add module.          |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_JS_DEL_MOD_FAILURE   | -8083                          | Unable to delete module.       |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_OLD_KRL              | -8082                          | New KRL is not later than the  |
   |                                |                                | current one.                   |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_CKL_CONFLICT         | -8081                          | New CKL has different issuer   |
   |                                |                                | than current CKL.              |
   +--------------------------------+--------------------------------+--------------------------------+
   | SE                             | -8080                          | Certificate issuer is not      |
   | C_ERROR_CERT_NOT_IN_NAME_SPACE |                                | permitted to issue a           |
   |                                |                                | certificate with this name.    |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_KRL_NOT_YET_VALID    | -8079                          | "The key revocation list for   |
   |                                |                                | this certificate is not yet    |
   |                                |                                | valid."                        |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_CRL_NOT_YET_VALID    | -8078                          | "The certificate revocation    |
   |                                |                                | list for this certificate is   |
   |                                |                                | not yet valid."                |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_UNKNOWN_CERT         | -8077                          | "The requested certificate     |
   |                                |                                | could not be found."           |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_UNKNOWN_SIGNER       | -8076                          | "The signer's certificate      |
   |                                |                                | could not be found."           |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_                           | -8075                          | "The location for the          |
   | ERROR_CERT_BAD_ACCESS_LOCATION |                                | certificate status server has  |
   |                                |                                | invalid format."               |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ER                         | -8074                          | "The OCSP response cannot be   |
   | ROR_OCSP_UNKNOWN_RESPONSE_TYPE |                                | fully decoded; it is of an     |
   |                                |                                | unknown type."                 |
   +--------------------------------+--------------------------------+--------------------------------+
   | SE                             | -8073                          | "The OCSP server returned      |
   | C_ERROR_OCSP_BAD_HTTP_RESPONSE |                                | unexpected/invalid HTTP data." |
   +--------------------------------+--------------------------------+--------------------------------+
   | SE                             | -8072                          | "The OCSP server found the     |
   | C_ERROR_OCSP_MALFORMED_REQUEST |                                | request to be corrupted or     |
   |                                |                                | improperly formed."            |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_OCSP_SERVER_ERROR    | -8071                          | "The OCSP server experienced   |
   |                                |                                | an internal error."            |
   +--------------------------------+--------------------------------+--------------------------------+
   | S                              | -8070                          | "The OCSP server suggests      |
   | EC_ERROR_OCSP_TRY_SERVER_LATER |                                | trying again later."           |
   +--------------------------------+--------------------------------+--------------------------------+
   | SE                             | -8069                          | "The OCSP server requires a    |
   | C_ERROR_OCSP_REQUEST_NEEDS_SIG |                                | signature on this request."    |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_E                          | -8068                          | "The OCSP server has refused   |
   | RROR_OCSP_UNAUTHORIZED_REQUEST |                                | this request as unauthorized." |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERRO                       | -8067                          | "The OCSP server returned an   |
   | R_OCSP_UNKNOWN_RESPONSE_STATUS |                                | unrecognizable status."        |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_OCSP_UNKNOWN_CERT    | -8066                          | "The OCSP server has no status |
   |                                |                                | for the certificate."          |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_OCSP_NOT_ENABLED     | -8065                          | "You must enable OCSP before   |
   |                                |                                | performing this operation."    |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_E                          | -8064                          | "You must set the OCSP default |
   | RROR_OCSP_NO_DEFAULT_RESPONDER |                                | responder before performing    |
   |                                |                                | this operation."               |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC                            | -8063                          | "The response from the OCSP    |
   | _ERROR_OCSP_MALFORMED_RESPONSE |                                | server was corrupted or        |
   |                                |                                | improperly formed."            |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ER                         | -8062                          | "The signer of the OCSP        |
   | ROR_OCSP_UNAUTHORIZED_RESPONSE |                                | response is not authorized to  |
   |                                |                                | give status for this           |
   |                                |                                | certificate."                  |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_OCSP_FUTURE_RESPONSE | -8061                          | "The OCSP response is not yet  |
   |                                |                                | valid (contains a date in the  |
   |                                |                                | future)."                      |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_OCSP_OLD_RESPONSE    | -8060                          | "The OCSP response contains    |
   |                                |                                | out-of-date information."      |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_DIGEST_NOT_FOUND     | -8059                          | "The CMS or PKCS #7 Digest was |
   |                                |                                | not found in signed message."  |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_                           | -8058                          | "The CMS or PKCS #7 Message    |
   | ERROR_UNSUPPORTED_MESSAGE_TYPE |                                | type is unsupported."          |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_MODULE_STUCK         | -8057                          | "PKCS #11 module could not be  |
   |                                |                                | removed because it is still in |
   |                                |                                | use."                          |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_BAD_TEMPLATE         | -8056                          | "Could not decode ASN.1 data.  |
   |                                |                                | Specified template was         |
   |                                |                                | invalid."                      |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_CRL_NOT_FOUND        | -8055                          | "No matching CRL was found."   |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_                           | -8054                          | "You are attempting to import  |
   | ERROR_REUSED_ISSUER_AND_SERIAL |                                | a cert with the same           |
   |                                |                                | issuer/serial as an existing   |
   |                                |                                | cert, but that is not the same |
   |                                |                                | cert."                         |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_BUSY                 | -8053                          | "NSS could not shutdown.       |
   |                                |                                | Objects are still in use."     |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_EXTRA_INPUT          | -8052                          | "DER-encoded message contained |
   |                                |                                | extra unused data."            |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ER                         | -8051                          | "Unsupported elliptic curve."  |
   | ROR_UNSUPPORTED_ELLIPTIC_CURVE |                                |                                |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_E                          | -8050                          | "Unsupported elliptic curve    |
   | RROR_UNSUPPORTED_EC_POINT_FORM |                                | point form."                   |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_UNRECOGNIZED_OID     | -8049                          | "Unrecognized Object           |
   |                                |                                | IDentifier."                   |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_E                          | -8048                          | "Invalid OCSP signing          |
   | RROR_OCSP_INVALID_SIGNING_CERT |                                | certificate in OCSP response." |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC                            | -8047                          | "Certificate is revoked in     |
   | _ERROR_REVOKED_CERTIFICATE_CRL |                                | issuer's certificate           |
   |                                |                                | revocation list."              |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_                           | -8046                          | "Issuer's OCSP responder       |
   | ERROR_REVOKED_CERTIFICATE_OCSP |                                | reports certificate is         |
   |                                |                                | revoked."                      |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_CRL_INVALID_VERSION  | -8045                          | "Issuer's Certificate          |
   |                                |                                | Revocation List has an unknown |
   |                                |                                | version number."               |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_E                          | -8044                          | "Issuer's V1 Certificate       |
   | RROR_CRL_V1_CRITICAL_EXTENSION |                                | Revocation List has a critical |
   |                                |                                | extension."                    |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_                     | -8043                          | "Issuer's V2 Certificate       |
   | CRL_UNKNOWN_CRITICAL_EXTENSION |                                | Revocation List has an unknown |
   |                                |                                | critical extension."           |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_UNKNOWN_OBJECT_TYPE  | -8042                          | "Unknown object type           |
   |                                |                                | specified."                    |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_INCOMPATIBLE_PKCS11  | -8041                          | "PKCS #11 driver violates the  |
   |                                |                                | spec in an incompatible way."  |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_NO_EVENT             | -8040                          | "No new slot event is          |
   |                                |                                | available at this time."       |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_CRL_ALREADY_EXISTS   | -8039                          | "CRL already exists."          |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_NOT_INITIALIZED      | -8038                          | "NSS is not initialized."      |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_TOKEN_NOT_LOGGED_IN  | -8037                          | "The operation failed because  |
   |                                |                                | the PKCS#11 token is not       |
   |                                |                                | logged in."                    |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERR                        | -8036                          | "The configured OCSP           |
   | OR_OCSP_RESPONDER_CERT_INVALID |                                | responder's certificate is     |
   |                                |                                | invalid."                      |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_OCSP_BAD_SIGNATURE   | -8035                          | "OCSP response has an invalid  |
   |                                |                                | signature."                    |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_OUT_OF_SEARCH_LIMITS | -8034                          | "Certification validation      |
   |                                |                                | search is out of search        |
   |                                |                                | limits."                       |
   +--------------------------------+--------------------------------+--------------------------------+
   | SE                             | -8033                          | "Policy mapping contains       |
   | C_ERROR_INVALID_POLICY_MAPPING |                                | any-policy."                   |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_                           | -8032                          | "Certificate chain fails       |
   | ERROR_POLICY_VALIDATION_FAILED |                                | policy validation."            |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_E                          | -8031                          | "Unknown location type in      |
   | RROR_UNKNOWN_AIA_LOCATION_TYPE |                                | certificate AIA extension."    |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_BAD_HTTP_RESPONSE    | -8030                          | "Server returned a bad HTTP    |
   |                                |                                | response."                     |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_BAD_LDAP_RESPONSE    | -8029                          | "Server returned a bad LDAP    |
   |                                |                                | response."                     |
   +--------------------------------+--------------------------------+--------------------------------+
   | S                              | -8028                          | "Failed to encode data with    |
   | EC_ERROR_FAILED_TO_ENCODE_DATA |                                | ASN.1 encoder."                |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_                           | -8027                          | "Bad information access        |
   | ERROR_BAD_INFO_ACCESS_LOCATION |                                | location in certificate        |
   |                                |                                | extension."                    |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_LIBPKIX_INTERNAL     | -8026                          | "Libpkix internal error        |
   |                                |                                | occurred during cert           |
   |                                |                                | validation."                   |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_PKCS11_GENERAL_ERROR | -8025                          | "A PKCS #11 module returned    |
   |                                |                                | CKR_GENERAL_ERROR, indicating  |
   |                                |                                | that an unrecoverable error    |
   |                                |                                | has occurred."                 |
   +--------------------------------+--------------------------------+--------------------------------+
   | SE                             | -8024                          | "A PKCS #11 module returned    |
   | C_ERROR_PKCS11_FUNCTION_FAILED |                                | CKR_FUNCTION_FAILED,           |
   |                                |                                | indicating that the requested  |
   |                                |                                | function could not be          |
   |                                |                                | performed. Trying the same     |
   |                                |                                | operation again might          |
   |                                |                                | succeed."                      |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_PKCS11_DEVICE_ERROR  | -8023                          | "A PKCS #11 module returned    |
   |                                |                                | CKR_DEVICE_ERROR, indicating   |
   |                                |                                | that a problem has occurred    |
   |                                |                                | with the token or slot."       |
   +--------------------------------+--------------------------------+--------------------------------+
   | SE                             | -8022                          | "Unknown information access    |
   | C_ERROR_BAD_INFO_ACCESS_METHOD |                                | method in certificate          |
   |                                |                                | extension."                    |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_CRL_IMPORT_FAILED    | -8021                          | "Error attempting to import a  |
   |                                |                                | CRL."                          |
   +--------------------------------+--------------------------------+--------------------------------+
   | SEC_ERROR_UNKNOWN_PKCS11_ERROR | -8018                          | "Unknown PKCS #11 error."      |
   |                                |                                | (unknown error value mapping)  |
   +--------------------------------+--------------------------------+--------------------------------+