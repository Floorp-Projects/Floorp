.. _mozilla_projects_nss_reference_nss_certificate_functions:

NSS Certificate Functions
=========================

.. _certificate_functions:

`Certificate Functions <#certificate_functions>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   This chapter describes the functions and related types used to work with a certificate database
   such as the cert8.db database provided with NSS. This was converted from `"Chapter 5: Certificate
   Functions" <https://www.mozilla.org/projects/security/pki/nss/ref/ssl/sslcrt.html>`__.

   -  :ref:`mozilla_projects_nss_reference`
   -  `Validating Certificates <NSS_Certificate_Functions#Validating_Certificates>`__
   -  `Manipulating Certificates <NSS_Certificate_Functions#Manipulating_Certificates>`__
   -  `Getting Certificate
      Information <NSS_Certificate_Functions#Getting_Certificate_Information>`__
   -  `Comparing SecItem Objects <NSS_Certificate_Functions#Comparing_SecItem_Objects>`__

   .. rubric:: Validating Certificates
      :name: validating_certificates

   -  `CERT_VerifyCertNow <NSS_Certificate_Functions#CERT_VerifyCertNow>`__
   -  `CERT_VerifyCert <NSS_Certificate_Functions#CERT_VerifyCert>`__
   -  `CERT_VerifyCertName <NSS_Certificate_Functions#CERT_VerifyCertName>`__
   -  `CERT_CheckCertValidTimes <NSS_Certificate_Functions#CERT_CheckCertValidTimes>`__
   -  `NSS_CmpCertChainWCANames <NSS_Certificate_Functions#NSS_CmpCertChainWCANames>`__

   .. rubric:: CERT_VerifyCertNow
      :name: cert_verifycertnow

   Checks that the current date is within the certificate's validity period and that the CA
   signature on the certificate is valid.

   .. rubric:: Syntax
      :name: syntax

   .. code:: eval

      #include <cert.h>

   .. code:: eval

      SECStatus CERT_VerifyCertNow(
        CERTCertDBHandle *handle,
        CERTCertificate *cert,
        PRBool checkSig,
        SECCertUsage certUsage,
        void *wincx);

   .. rubric:: Parameters
      :name: parameters

   This function has the following parameters:

   *handle*\ A pointer to the certificate database handle.

   *cert*\ A pointer to the certificate to be checked.

   *checkSig*\ Indicates whether certificate signatures are to be checked.

   -  PR_TRUE means certificate signatures are to be checked.
   -  PR_FALSE means certificate signatures will not be checked.

   *certUsage*\ One of these values:

   -  certUsageSSLClient
   -  certUsageSSLServer
   -  certUsageSSLServerWithStepUp
   -  certUsageSSLCA
   -  certUsageEmailSigner
   -  certUsageEmailRecipient
   -  certUsageObjectSigner
   -  certUsageUserCertImport
   -  certUsageVerifyCA
   -  certUsageProtectedObjectSigner

   *wincx*\ The PIN argument value to pass to PK11 functions. See description below for more
   information.

   .. rubric:: Returns
      :name: returns

   The function returns one of these values:

   -  If successful, SECSuccess.
   -  If unsuccessful, SECFailure. Use PR_GetError to obtain the error code.

   .. rubric:: Description
      :name: description

   The CERT_VerifyCertNow function must call one or more PK11 functions to obtain the services of a
   PKCS #11 module. Some of the PK11 functions require a PIN argument (see SSL_SetPKCS11PinArg for
   details), which must be specified in the wincx parameter. To obtain the value to pass in the
   wincx parameter, call SSL_RevealPinArg.

   .. rubric:: CERT_VerifyCert
      :name: cert_verifycert

   Checks that the a given aribrary date is within the certificate's validity period and that the CA
   signature on the certificate is valid. It also optionally returns a log of all the problems with
   the chain. Calling CERT_VerifyCert with the parameters: CERT_VerifyCert(handle, cert, checkSig,
   certUsage, PR_Now(), wincx, NULL) is equivalent to calling CERT_VerifyNow(handle, cert, checkSig,
   certUsage, wincx).

   .. rubric:: Syntax
      :name: syntax_2

   .. code:: eval

      #include <cert.h>

   .. code:: eval

      SECStatus CERT_VerifyCert(
        CERTCertDBHandle *handle,
        CERTCertificate *cert,
        PRBool checkSig,
        SECCertUsage certUsage,
        int 64 t,
        void *wincx
        CERTVerifyLog *log);

   .. rubric:: Parameters
      :name: parameters_2

   This function has the following parameters:

   *handle*\ A pointer to the certificate database handle.

   *cert*\ A pointer to the certificate to be checked.

   *checkSig*\ Indicates whether certificate signatures are to be checked.

   -  PR_TRUE means certificate signatures are to be checked.
   -  PR_FALSE means certificate signatures will not be checked.

   *certUsage*\ One of these values:

   -  certUsageSSLClient
   -  certUsageSSLServer
   -  certUsageSSLServerWithStepUp
   -  certUsageSSLCA
   -  certUsageEmailSigner
   -  certUsageEmailRecipient
   -  certUsageObjectSigner
   -  certUsageUserCertImport
   -  certUsageVerifyCA
   -  certUsageProtectedObjectSigner

   *t*\ Time in which to validate the certificate.

   *wincx*\ The PIN argument value to pass to PK11 functions. See description below for more
   information.

   *log*\ Optional certificate log which returns all the errors in processing a given certificate
   chain. See :ref:`mozilla_projects_nss_certverify_log` for more information.

   .. rubric:: Returns
      :name: returns_2

   The function returns one of these values:

   -  If successful, SECSuccess.
   -  If unsuccessful, SECFailure. Use PR_GetError to obtain the error code.

   .. rubric:: Description
      :name: description_2

   The CERT_VerifyCert function must call one or more PK11 functions to obtain the services of a
   PKCS #11 module. Some of the PK11 functions require a PIN argument (see SSL_SetPKCS11PinArg for
   details), which must be specified in the wincx parameter. To obtain the value to pass in the
   wincx parameter, call SSL_RevealPinArg.

   .. rubric:: CERT_VerifyCertName
      :name: cert_verifycertname

   Compares the common name specified in the subject DN for a certificate with a specified hostname.

   .. rubric:: Syntax
      :name: syntax_3

   .. code:: eval

      #include <cert.h>

   .. code:: eval

      SECStatus CERT_VerifyCertName(
        CERTCertificate *cert,
        char *hostname);

   .. rubric:: Parameters
      :name: parameters_3

   This function has the following parameters:

   *cert*\ A pointer to the certificate against which to check the hostname referenced by hostname.

   *hostname*\ The hostname to be checked.

   .. rubric:: Returns
      :name: returns_3

   The function returns one of these values:

   -  If the common name in the subject DN for the certificate matches the domain name passed in the
      hostname parameter, SECSuccess.
   -  If the common name in the subject DN for the certificate is not identical to the domain name
      passed in the hostname parameter, SECFailure. Use PR_GetError to obtain the error code.

   .. rubric:: Description
      :name: description_3

   The comparison performed by CERT_VerifyCertName is not a simple string comparison. Instead, it
   takes account of the following rules governing the construction of common names in SSL server
   certificates:

   -  \* matches anything
   -   ? matches one character
   -  \\ escapes a special character
   -  $ matches the end of the string
   -  [abc] matches one occurrence of a, b, or c. The only character that needs to be escaped in
      this is ], all others are not special.
   -  [a-z] matches any character between a and z
   -  [^az] matches any character except a or z
   -  ~ followed by another shell expression removes any pattern matching the shell expression from
      the match list
   -  (foo|bar) matches either the substring foo or the substring bar. These can be shell
      expressions as well.

   .. rubric:: CERT_CheckCertValidTimes
      :name: cert_checkcertvalidtimes

   Checks whether a specified time is within a certificate's validity period.

   .. rubric:: Syntax
      :name: syntax_4

   .. code:: eval

      #include <cert.h>
      #include <certt.h>

   .. code:: eval

      SECCertTimeValidity CERT_CheckCertValidTimes(
        CERTCertificate *cert,
        int64 t);

   .. rubric:: Parameters
      :name: parameters_4

   This function has the following parameters:

   *cert*\ A pointer to the certificate whose validity period you want to check against.

   *t*\ The time to check against the certificate's validity period. For more information, see the
   NSPR header pr_time.h.

   .. rubric:: Returns
      :name: returns_4

   The function returns an enumerator of type SECCertTimeValidity:

   .. code:: eval

      typedef enum {
        secCertTimeValid,
        secCertTimeExpired,
        secCertTimeNotValidYet
      } SECCertTimeValidity;

   .. rubric:: NSS_CmpCertChainWCANames
      :name: nss_cmpcertchainwcanames

   Determines whether any of the signers in the certificate chain for a specified certificate are on
   a specified list of CA names.

   .. rubric:: Syntax
      :name: syntax_5

   .. code:: eval

      #include <nss.h>

      SECStatus NSS_CmpCertChainWCANames(
        CERTCertificate *cert,
        CERTDistNames *caNames);

   .. rubric:: Parameters
      :name: parameters_5

   This function has the following parameters:

   *cert*\ A pointer to the certificate structure for the certificate whose certificate chain is to
   be checked.

   *caNames*\ A pointer to a structure that contains a list of distinguished names (DNs) against
   which to check the DNs for the signers in the certificate chain.

   .. rubric:: Returns
      :name: returns_5

   The function returns one of these values:

   -  If successful, SECSuccess.
   -  If unsuccessful, SECFailure. Use PR_GetError to obtain the error code.

   .. rubric:: Manipulating Certificates
      :name: manipulating_certificates

   -  `CERT_DupCertificate <#cert_dupcertificate>`__
   -  `CERT_DestroyCertificate <#cert_destroycertificate>`__

   .. rubric:: CERT_DupCertificate
      :name: cert_dupcertificate

   Makes a shallow copy of a specified certificate.

   .. rubric:: Syntax
      :name: syntax_6

   .. code:: eval

      #include <cert.h>

   .. code:: eval

      CERTCertificate *CERT_DupCertificate(CERTCertificate *c)

   .. rubric:: Parameter
      :name: parameter

   This function has the following parameter:

   *c*\ A pointer to the certificate object to be duplicated.

   .. rubric:: Returns
      :name: returns_6

   If successful, the function returns a pointer to a certificate object of type CERTCertificate.

   .. rubric:: Description
      :name: description_4

   The CERT_DupCertificate function increments the reference count for the certificate passed in the
   c parameter.

   .. rubric:: CERT_DestroyCertificate
      :name: cert_destroycertificate

   Destroys a certificate object.

   .. rubric:: Syntax
      :name: syntax_7

   .. code:: eval

      #include <cert.h>
      #include <certt.h>

   .. code:: eval

      void CERT_DestroyCertificate(CERTCertificate *cert);

   .. rubric:: Parameters
      :name: parameters_6

   This function has the following parameter:

   *cert*\ A pointer to the certificate to destroy.

   .. rubric:: Description
      :name: description_5

   Certificate and key structures are shared objects. When an application makes a copy of a
   particular certificate or key structure that already exists in memory, SSL makes a shallow
   copy--that is, it increments the reference count for that object rather than making a whole new
   copy. When you call CERT_DestroyCertificate or SECKEY_DestroyPrivateKey, the function decrements
   the reference count and, if the reference count reaches zero as a result, both frees the memory
   and sets all the bits to zero. The use of the word "destroy" in function names or in the
   description of a function implies reference counting.

   Never alter the contents of a certificate or key structure. If you attempt to do so, the change
   affects all the shallow copies of that structure and can cause severe problems.

   .. rubric:: Getting Certificate Information
      :name: getting_certificate_information

   -  `CERT_FindCertByName <#cert_findcertbyname>`__
   -  `CERT_GetCertNicknames <#cert_getcertnicknames>`__
   -  `CERT_FreeNicknames <#cert_freenicknames>`__
   -  `CERT_GetDefaultCertDB <#cert_getdefaultcertdb>`__
   -  `NSS_FindCertKEAType <#nss_findcertkeatype>`__

   .. rubric:: CERT_FindCertByName
      :name: cert_findcertbyname

   Finds the certificate in the certificate database with a specified DN.

   .. rubric:: Syntax
      :name: syntax_8

   .. code:: eval

      #include <cert.h>

   .. code:: eval

      CERTCertificate *CERT_FindCertByName (
        CERTCertDBHandle *handle,
        SECItem *name);

   .. rubric:: Parameters
      :name: parameters_7

   This function has the following parameters:

   *handle*\ A pointer to the certificate database handle.

   *name*\ The subject DN of the certificate you wish to find.

   .. rubric:: Returns
      :name: returns_7

   If successful, the function returns a certificate object of type CERTCertificate.

   .. rubric:: CERT_GetCertNicknames
      :name: cert_getcertnicknames

   Returns the nicknames of the certificates in a specified certificate database.

   .. rubric:: Syntax
      :name: syntax_9

   .. code:: eval

      #include <cert.h>
      #include <certt.h>

   .. code:: eval

      CERTCertNicknames *CERT_GetCertNicknames (
        CERTCertDBHandle *handle,
        int what,
        void *wincx);

   .. rubric:: Parameters
      :name: parameters_8

   This function has the following parameters:

   *handle*\ A pointer to the certificate database handle.

   *what*\ One of these values:

   -  SEC_CERT_NICKNAMES_ALL
   -  SEC_CERT_NICKNAMES_USER
   -  SEC_CERT_NICKNAMES_SERVER
   -  SEC_CERT_NICKNAMES_CA

   *wincx*\ The PIN argument value to pass to PK11 functions. See description below for more
   information.

   .. rubric:: Returns
      :name: returns_8

   The function returns a CERTCertNicknames object containing the requested nicknames.

   .. rubric:: Description
      :name: description_6

   CERT_GetCertNicknames must call one or more PK11 functions to obtain the services of a PKCS #11
   module. Some of the PK11 functions require a PIN argument (see SSL_SetPKCS11PinArg for details),
   which must be specified in the wincx parameter. To obtain the value to pass in the wincx
   parameter, call SSL_RevealPinArg.

   .. rubric:: CERT_FreeNicknames
      :name: cert_freenicknames

   Frees a CERTCertNicknames structure. This structure is returned by CERT_GetCertNicknames.

   .. rubric:: Syntax
      :name: syntax_10

   .. code:: eval

      #include <cert.h>

   .. code:: eval

      void CERT_FreeNicknames(CERTCertNicknames *nicknames);

   .. rubric:: Parameters
      :name: parameters_9

   This function has the following parameter:

   *nicknames*\ A pointer to the CERTCertNicknames structure to be freed.

   .. rubric:: CERT_GetDefaultCertDB
      :name: cert_getdefaultcertdb

   Returns a handle to the default certificate database.

   .. rubric:: Syntax
      :name: syntax_11

   .. code:: eval

      #include <cert.h>

   .. code:: eval

      CERTCertDBHandle *CERT_GetDefaultCertDB(void);

   .. rubric:: Returns
      :name: returns_9

   The function returns the CERTCertDBHandle for the default certificate database.

   .. rubric:: Description
      :name: description_7

   This function is useful for determining whether the default certificate database has been opened.

   .. rubric:: NSS_FindCertKEAType
      :name: nss_findcertkeatype

   Returns key exchange type of the keys in an SSL server certificate.

   .. rubric:: Syntax
      :name: syntax_12

   .. code:: eval

      #include <nss.h>

   .. code:: eval

      SSLKEAType NSS_FindCertKEAType(CERTCertificate * cert);

   .. rubric:: Parameter
      :name: parameter_2

   This function has the following parameter:

   *a*\ The certificate to check.

   .. rubric:: Returns
      :name: returns_10

   The function returns one of these values:

   -  kt_null = 0
   -  kt_rsa
   -  kt_dh
   -  kt_fortezza
   -  kt_kea_size

   .. rubric:: Comparing SecItem Objects
      :name: comparing_secitem_objects

   .. rubric:: SECITEM_CompareItem
      :name: secitem_compareitem

   Compares two SECItem objects and returns a SECComparison enumerator that shows the difference
   between them.

   .. rubric:: Syntax
      :name: syntax_13

   .. code:: eval

      #include <secitem.h>
      #include <seccomon.h>

   .. code:: eval

      SECComparison SECITEM_CompareItem(
        SECItem *a,
        SECItem *b);

   .. rubric:: Parameters
      :name: parameters_10

   This function has the following parameters:

   *a*\ A pointer to one of the items to be compared.

   *b*\ A pointer to one of the items to be compared.

   .. rubric:: Returns
      :name: returns_11

   The function returns an enumerator of type SECComparison.

   .. code:: eval

      typedef enum _SECComparison {
        SECLessThan                = -1,
        SECEqual                = 0,
        SECGreaterThan = 1
      } SECComparison;