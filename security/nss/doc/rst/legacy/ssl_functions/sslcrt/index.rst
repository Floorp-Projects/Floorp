.. _mozilla_projects_nss_ssl_functions_sslcrt:

sslcrt
======

.. container::

   .. note::

      -  This page is part of the :ref:`mozilla_projects_nss_ssl_functions_old_ssl_reference` that
         we are migrating into the format described in the `MDN Style
         Guide <https://developer.mozilla.org/en-US/docs/Project:MDC_style_guide>`__. If you are
         inclined to help with this migration, your help would be very much appreciated.

      -  Upgraded documentation may be found in the :ref:`mozilla_projects_nss_reference`

   .. rubric:: Certificate Functions
      :name: Certificate_Functions

   --------------

.. _chapter_5_certificate_functions:

`Chapter 5
 <#chapter_5_certificate_functions>`__ Certificate Functions
------------------------------------------------------------

.. container::

   This chapter describes the functions and related types used to work with a certificate database
   such as the ``cert7.db`` database provided with Communicator.

   |  `Validating Certificates <#1060423>`__
   | `Manipulating Certificates <#1056436>`__
   | `Getting Certificate Information <#1056475>`__
   | `Comparing SecItem Objects <#1055384>`__

.. _validating_certificates:

`Validating Certificates <#validating_certificates>`__
------------------------------------------------------

.. container::

   |  ```CERT_VerifyCertNow`` <#1058011>`__
   | ```CERT_VerifyCertName`` <#1050342>`__
   | ```CERT_CheckCertValidTimes`` <#1056662>`__
   | ```NSS_CmpCertChainWCANames`` <#1056760>`__

   .. rubric:: CERT_VerifyCertNow
      :name: cert_verifycertnow

   Checks that the current date is within the certificate's validity period and that the CA
   signature on the certificate is valid.

   .. rubric:: Syntax
      :name: syntax

   .. code:: notranslate

      #include <cert.h> 

   .. code:: notranslate

      SECStatus CERT_VerifyCertNow(
         CERTCertDBHandle *handle,
         CERTCertificate *cert,
         PRBool checkSig,
         SECCertUsage certUsage,
         void *wincx);

   .. rubric:: Parameters
      :name: parameters

   This function has the following parameters:

   +-------------------------------------------------+-------------------------------------------------+
   | .. code:: notranslate                           | A pointer to the certificate database handle.   |
   |                                                 |                                                 |
   |    handle                                       |                                                 |
   +-------------------------------------------------+-------------------------------------------------+
   | .. code:: notranslate                           | A pointer to the certificate to be checked.     |
   |                                                 |                                                 |
   |    cert                                         |                                                 |
   +-------------------------------------------------+-------------------------------------------------+
   | .. code:: notranslate                           | Indicates whether certificate signatures are to |
   |                                                 | be checked. ``PR_TRUE`` means certificate       |
   |    checkSig                                     | signatures are to be checked. ``PR_FALSE``      |
   |                                                 | means certificate signatures will not be        |
   |                                                 | checked.                                        |
   +-------------------------------------------------+-------------------------------------------------+
   | .. code:: notranslate                           | One of these values:                            |
   |                                                 |                                                 |
   |    certUsage                                    | -  ``certUsageSSLClient``                       |
   |                                                 | -  ``certUsageSSLServer``                       |
   |                                                 | -  ``certUsageSSLServerWithStepUp``             |
   |                                                 | -  ``certUsageSSLCA``                           |
   |                                                 | -  ``certUsageEmailSigner``                     |
   |                                                 | -  ``certUsageEmailRecipient``                  |
   |                                                 | -  ``certUsageObjectSigner``                    |
   |                                                 | -  ``certUsageUserCertImport``                  |
   |                                                 | -  ``certUsageVerifyCA``                        |
   |                                                 | -  ``certUsageProtectedObjectSigner``           |
   +-------------------------------------------------+-------------------------------------------------+
   | .. code:: notranslate                           | The PIN argument value to pass to PK11          |
   |                                                 | functions. See description below for more       |
   |    wincx                                        | information.                                    |
   +-------------------------------------------------+-------------------------------------------------+

   .. rubric:: Returns
      :name: returns

   The function returns one of these values:

   -  If successful, ``SECSuccess``.
   -  If unsuccessful, ``SECFailure``. Use
      ```PR_GetError`` <../../../../../nspr/reference/html/prerr.html#26127>`__ to obtain the error
      code.

   .. rubric:: Description
      :name: description

   The ``CERT_VerifyCertNow`` function must call one or more PK11 functions to obtain the services
   of a PKCS #11 module. Some of the PK11 functions require a PIN argument (see
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1088040` for details), which must be specified in
   the ``wincx`` parameter. To obtain the value to pass in the ``wincx`` parameter, call
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1123385`.

   .. rubric:: CERT_VerifyCertName
      :name: cert_verifycertname

   Compares the common name specified in the subject DN for a certificate with a specified hostname.

   .. rubric:: Syntax
      :name: syntax_2

   .. code:: notranslate

      #include <cert.h>

   .. code:: notranslate

      SECStatus CERT_VerifyCertName(
         CERTCertificate *cert,
         char *hostname);

   .. rubric:: Parameters
      :name: parameters_2

   This function has the following parameters:

   +-------------------------------------------------+-------------------------------------------------+
   | .. code:: notranslate                           | A pointer to the certificate against which to   |
   |                                                 | check the hostname referenced by ``hostname``.  |
   |    cert                                         |                                                 |
   +-------------------------------------------------+-------------------------------------------------+
   | .. code:: notranslate                           | The hostname to be checked.                     |
   |                                                 |                                                 |
   |    hostname                                     |                                                 |
   +-------------------------------------------------+-------------------------------------------------+

   .. rubric:: Returns
      :name: returns_2

   The function returns one of these values:

   -  If the common name in the subject DN for the certificate matches the domain name passed in the
      ``hostname`` parameter, ``SECSuccess``.
   -  If the common name in the subject DN for the certificate is not identical to the domain name
      passed in the ``hostname`` parameter, ``SECFailure``. Use
      ```PR_GetError`` <../../../../../nspr/reference/html/prerr.html#26127>`__ to obtain the error
      code.

   .. rubric:: Description
      :name: description_2

   The comparison performed by CERT_VerifyCertName is not a simple string comparison. Instead, it
   takes account of the following rules governing the construction of common names in SSL server
   certificates:

   -  ``*`` matches anything
   -  ``?`` matches one character
   -  ``\`` escapes a special character
   -  ``$`` matches the end of the string
   -  ``[abc]`` matches one occurrence of ``a``, ``b``, or ``c``. The only character that needs to
      be escaped in this is ``]``, all others are not special.
   -  ``[a-z]`` matches any character between ``a`` and ``z``
   -  ``[^az]`` matches any character except ``a`` or ``z``
   -  ``~`` followed by another shell expression removes any pattern matching the shell expression
      from the match list
   -  ``(foo|bar)`` matches either the substring ``foo`` or the substring ``bar``. These can be
      shell expressions as well.

   .. rubric:: CERT_CheckCertValidTimes
      :name: cert_checkcertvalidtimes

   Checks whether a specified time is within a certificate's validity period.

   .. rubric:: Syntax
      :name: syntax_3

   .. code:: notranslate

      #include <cert.h>
      #include <certt.h>

   .. code:: notranslate

      SECCertTimeValidity CERT_CheckCertValidTimes(
         CERTCertificate *cert,
         int64 t);

   .. rubric:: Parameters
      :name: parameters_3

   This function has the following parameters:

   +-------------------------------------------------+-------------------------------------------------+
   | .. code:: notranslate                           | A pointer to the certificate whose validity     |
   |                                                 | period you want to check against.               |
   |    cert                                         |                                                 |
   +-------------------------------------------------+-------------------------------------------------+
   | .. code:: notranslate                           | The time to check against the certificate's     |
   |                                                 | validity period. For more information, see the  |
   |    t                                            | NSPR header ``pr_time.h``.                      |
   +-------------------------------------------------+-------------------------------------------------+

   .. rubric:: Returns
      :name: returns_3

   The function returns an enumerator of type ``SECCertTimeValidity``:

   .. code:: notranslate

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
      :name: syntax_4

   .. code:: notranslate

      #include <nss.h>

   .. code:: notranslate

      SECStatus NSS_CmpCertChainWCANames(
         CERTCertificate *cert,
         CERTDistNames *caNames);

   .. rubric:: Parameters
      :name: parameters_4

   This function has the following parameters:

   +-------------------------------------------------+-------------------------------------------------+
   | .. code:: notranslate                           | A pointer to the certificate structure for the  |
   |                                                 | certificate whose certificate chain is to be    |
   |    cert                                         | checked.                                        |
   +-------------------------------------------------+-------------------------------------------------+
   | .. code:: notranslate                           | A pointer to a structure that contains a list   |
   |                                                 | of distinguished names (DNs) against which to   |
   |    caNames                                      | check the DNs for the signers in the            |
   |                                                 | certificate chain.                              |
   +-------------------------------------------------+-------------------------------------------------+

   .. rubric:: Returns
      :name: returns_4

   The function returns one of these values:

   -  If successful, ``SECSuccess``.
   -  If unsuccessful, ``SECFailure``. Use
      ```PR_GetError`` <../../../../../nspr/reference/html/prerr.html#26127>`__ to obtain the error
      code.

.. _manipulating_certificates:

`Manipulating Certificates <#manipulating_certificates>`__
----------------------------------------------------------

.. container::

   |  ```CERT_DupCertificate`` <#1058344>`__
   | ```CERT_DestroyCertificate`` <#1050532>`__

   .. rubric:: CERT_DupCertificate
      :name: cert_dupcertificate

   Makes a shallow copy of a specified certificate.

   .. rubric:: Syntax
      :name: syntax_5

   .. code:: notranslate

      #include <cert.h>

   .. code:: notranslate

      CERTCertificate *CERT_DupCertificate(CERTCertificate *c)

   .. rubric:: Parameter
      :name: parameter

   This function has the following parameter:

   +-------------------------------------------------+-------------------------------------------------+
   | .. code:: notranslate                           | A pointer to the certificate object to be       |
   |                                                 | duplicated.                                     |
   |    c                                            |                                                 |
   +-------------------------------------------------+-------------------------------------------------+

   .. rubric:: Returns
      :name: returns_5

   If successful, the function returns a pointer to a certificate object of type
   ```CERTCertificate`` <ssltyp.html#1027387>`__.

   .. rubric:: Description
      :name: description_3

   The ``CERT_DupCertificate`` function increments the reference count for the certificate passed in
   the ``c`` parameter.

   .. rubric:: CERT_DestroyCertificate
      :name: cert_destroycertificate

   Destroys a certificate object.

   .. rubric:: Syntax
      :name: syntax_6

   .. code:: notranslate

      #include <cert.h>
      #include <certt.h>

   .. code:: notranslate

      void CERT_DestroyCertificate(CERTCertificate *cert);

   .. rubric:: Parameters
      :name: parameters_5

   This function has the following parameter:

   +-------------------------------------------------+-------------------------------------------------+
   | .. code:: notranslate                           | A pointer to the certificate to destroy.        |
   |                                                 |                                                 |
   |    cert                                         |                                                 |
   +-------------------------------------------------+-------------------------------------------------+

   .. rubric:: Description
      :name: description_4

   Certificate and key structures are shared objects. When an application makes a copy of a
   particular certificate or key structure that already exists in memory, SSL makes a *shallow*
   copy--that is, it increments the reference count for that object rather than making a whole new
   copy. When you call ```CERT_DestroyCertificate`` <#1050532>`__ or
   ```SECKEY_DestroyPrivateKey`` <sslkey.html#1051017>`__, the function decrements the reference
   count and, if the reference count reaches zero as a result, both frees the memory and sets all
   the bits to zero. The use of the word "destroy" in function names or in the description of a
   function implies reference counting.

   Never alter the contents of a certificate or key structure. If you attempt to do so, the change
   affects all the shallow copies of that structure and can cause severe problems.

.. _getting_certificate_information:

`Getting Certificate Information <#getting_certificate_information>`__
----------------------------------------------------------------------

.. container::

   |  ```CERT_FindCertByName`` <#1050345>`__
   | ```CERT_GetCertNicknames`` <#1050346>`__
   | ```CERT_FreeNicknames`` <#1050349>`__
   | ```CERT_GetDefaultCertDB`` <#1052308>`__
   | ```NSS_FindCertKEAType`` <#1056950>`__

   .. rubric:: CERT_FindCertByName
      :name: cert_findcertbyname

   Finds the certificate in the certificate database with a specified DN.

   .. rubric:: Syntax
      :name: syntax_7

   .. code:: notranslate

      #include <cert.h>

   .. code:: notranslate

      CERTCertificate *CERT_FindCertByName (
         CERTCertDBHandle *handle,
         SECItem *name);

   .. rubric:: Parameters
      :name: parameters_6

   This function has the following parameters:

   +-------------------------------------------------+-------------------------------------------------+
   | .. code:: notranslate                           | A pointer to the certificate database handle.   |
   |                                                 |                                                 |
   |    handle                                       |                                                 |
   +-------------------------------------------------+-------------------------------------------------+
   | .. code:: notranslate                           | The subject DN of the certificate you wish to   |
   |                                                 | find.                                           |
   |    name                                         |                                                 |
   +-------------------------------------------------+-------------------------------------------------+

   .. rubric:: Returns
      :name: returns_6

   If successful, the function returns a certificate object of type
   ```CERTCertificate`` <ssltyp.html#1027387>`__.

   .. rubric:: CERT_GetCertNicknames
      :name: cert_getcertnicknames

   Returns the nicknames of the certificates in a specified certificate database.

   .. rubric:: Syntax
      :name: syntax_8

   .. code:: notranslate

      #include <cert.h>
      #include <certt.h>

   .. code:: notranslate

      CERTCertNicknames *CERT_GetCertNicknames (
         CERTCertDBHandle *handle,
         int what,
         void *wincx);

   .. rubric:: Parameters
      :name: parameters_7

   This function has the following parameters:

   +-------------------------------------------------+-------------------------------------------------+
   | .. code:: notranslate                           | A pointer to the certificate database handle.   |
   |                                                 |                                                 |
   |    handle                                       |                                                 |
   +-------------------------------------------------+-------------------------------------------------+
   | .. code:: notranslate                           | One of these values:                            |
   |                                                 |                                                 |
   |    what                                         | -  ``SEC_CERT_NICKNAMES_ALL``                   |
   |                                                 | -  ``SEC_CERT_NICKNAMES_USER``                  |
   |                                                 | -  ``SEC_CERT_NICKNAMES_SERVER``                |
   |                                                 | -  ``SEC_CERT_NICKNAMES_CA``                    |
   +-------------------------------------------------+-------------------------------------------------+
   | .. code:: notranslate                           | The PIN argument value to pass to PK11          |
   |                                                 | functions. See description below for more       |
   |    wincx                                        | information.                                    |
   +-------------------------------------------------+-------------------------------------------------+

   .. rubric:: Returns
      :name: returns_7

   The function returns a ``CERTCertNicknames`` object containing the requested nicknames.

   .. rubric:: Description
      :name: description_5

   ``CERT_GetCertNicknames`` must call one or more PK11 functions to obtain the services of a PKCS
   #11 module. Some of the PK11 functions require a PIN argument (see
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1088040` for details), which must be specified in
   the ``wincx`` parameter. To obtain the value to pass in the ``wincx`` parameter, call
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1123385`.

   .. rubric:: CERT_FreeNicknames
      :name: cert_freenicknames

   Frees a ``CERTCertNicknames`` structure. This structure is returned by
   ```CERT_GetCertNicknames`` <#1050346>`__.

   .. rubric:: Syntax
      :name: syntax_9

   .. code:: notranslate

      #include <cert.h>

   .. code:: notranslate

      void CERT_FreeNicknames(CERTCertNicknames *nicknames);

   .. rubric:: Parameters
      :name: parameters_8

   This function has the following parameter:

   +-------------------------------------------------+-------------------------------------------------+
   | .. code:: notranslate                           | A pointer to the ``CERTCertNicknames``          |
   |                                                 | structure to be freed.                          |
   |    nicknames                                    |                                                 |
   +-------------------------------------------------+-------------------------------------------------+

   .. rubric:: CERT_GetDefaultCertDB
      :name: cert_getdefaultcertdb

   Returns a handle to the default certificate database.

   .. rubric:: Syntax
      :name: syntax_10

   .. code:: notranslate

      #include <cert.h>

   .. code:: notranslate

      CERTCertDBHandle *CERT_GetDefaultCertDB(void);

   .. rubric:: Returns
      :name: returns_8

   The function returns the ```CERTCertDBHandle`` <ssltyp.html#1028465>`__ for the default
   certificate database.

   .. rubric:: Description
      :name: description_6

   This function is useful for determining whether the default certificate database has been opened.

   .. rubric:: NSS_FindCertKEAType
      :name: nss_findcertkeatype

   Returns key exchange type of the keys in an SSL server certificate.

   .. rubric:: Syntax
      :name: syntax_11

   .. code:: notranslate

      #include <nss.h>

   .. code:: notranslate

      SSLKEAType NSS_FindCertKEAType(CERTCertificate * cert);

   .. rubric:: Parameter
      :name: parameter_2

   This function has the following parameter:

   +-------------------------------------------------+-------------------------------------------------+
   | .. code:: notranslate                           | The certificate to check.                       |
   |                                                 |                                                 |
   |    a                                            |                                                 |
   +-------------------------------------------------+-------------------------------------------------+

   .. rubric:: Returns
      :name: returns_9

   The function returns one of these values:

   -  ``kt_null = 0``
   -  ``kt_rsa``
   -  ``kt_dh``
   -  ``kt_fortezza``
   -  ``kt_kea_size``

.. _comparing_secitem_objects:

`Comparing SecItem Objects <#comparing_secitem_objects>`__
----------------------------------------------------------

.. container::

   .. rubric:: SECITEM_CompareItem
      :name: secitem_compareitem

   Compares two ```SECItem`` <ssltyp.html#1026076>`__ objects and returns a ``SECComparison``
   enumerator that shows the difference between them.

   .. rubric:: Syntax
      :name: syntax_12

   .. code:: notranslate

      #include <secitem.h>
      #include <seccomon.h>

   .. code:: notranslate

      SECComparison SECITEM_CompareItem(
         SECItem *a,
         SECItem *b);

   .. rubric:: Parameters
      :name: parameters_9

   This function has the following parameters:

   +-------------------------------------------------+-------------------------------------------------+
   | .. code:: notranslate                           | A pointer to one of the items to be compared.   |
   |                                                 |                                                 |
   |    a                                            |                                                 |
   +-------------------------------------------------+-------------------------------------------------+
   | .. code:: notranslate                           | A pointer to one of the items to be compared.   |
   |                                                 |                                                 |
   |    b                                            |                                                 |
   +-------------------------------------------------+-------------------------------------------------+

   .. rubric:: Returns
      :name: returns_10

   The function returns an enumerator of type ``SECComparison``.

   .. code:: notranslate

      typedef enum _SECComparison {
         SECLessThan                = -1,
         SECEqual                = 0,
         SECGreaterThan = 1
      } SECComparison;