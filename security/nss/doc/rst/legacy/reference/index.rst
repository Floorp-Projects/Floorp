.. _mozilla_projects_nss_reference:

NSS reference
=============

.. _initial_notes:

`Initial Notes <#initial_notes>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   .. container:: notecard note

      -  We are migrating the :ref:`mozilla_projects_nss_ssl_functions_old_ssl_reference` into the
         format described in the `MDN Style
         Guide <https://developer.mozilla.org/en-US/docs/MDN/Guidelines>`__. If you are inclined to
         help with this migration, your help would be very much appreciated.

      -  The proposed chapters below are based on the chapters of the
         :ref:`mozilla_projects_nss_ssl_functions_old_ssl_reference` and the categories of functions
         in :ref:`mozilla_projects_nss_reference_nss_functions`.

      -  Should a particular page require the use of an underscore, please see the documentation for
         the `Title Override Extension </Project:En/MDC_style_guide#Title_Override_Extension>`__.

.. _building_and_installing_nss:

`Building and installing NSS <#building_and_installing_nss>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   :ref:`mozilla_projects_nss_reference_building_and_installing_nss`

.. _overview_of_an_nss_application:

`Overview of an NSS application <#overview_of_an_nss_application>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Based on :ref:`mozilla_projects_nss_ssl_functions_sslintro` in the SSL Reference.

.. _getting_started_with_nss:

`Getting started with NSS <#getting_started_with_nss>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Based on :ref:`mozilla_projects_nss_ssl_functions_gtstd` in the SSL Reference.

.. _data_types:

`Data types <#data_types>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Based on :ref:`mozilla_projects_nss_ssl_functions_ssltyp` in the SSL Reference.

.. _nss_initialization_and_shutdown:

`NSS initialization and shutdown <#nss_initialization_and_shutdown>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  NSS_Init
   -  NSS_InitReadWrite
   -  NSS_NoDB_Init
   -  :ref:`mozilla_projects_nss_reference_nss_initialize`
   -  NSS_Shutdown

.. _utility_functions:

`Utility functions <#utility_functions>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Based on :ref:`mozilla_projects_nss_reference_nss_functions#utility_functions` in NSS Public
   Functions.

.. _certificate_functions:

`Certificate functions <#certificate_functions>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Based on :ref:`mozilla_projects_nss_ssl_functions_sslcrt` in the SSL Reference and
   :ref:`mozilla_projects_nss_reference_nss_functions#certificate_functions` in NSS Public
   Functions.

   -  :ref:`mozilla_projects_nss_reference_nss_certificate_functions#validating_certificates`

      -  :ref:`mozilla_projects_nss_reference_nss_certificate_functions#cert_verifycertnow`
      -  :ref:`mozilla_projects_nss_reference_nss_certificate_functions#cert_verifycert`
      -  :ref:`mozilla_projects_nss_reference_nss_certificate_functions#cert_verifycertname`
      -  :ref:`mozilla_projects_nss_reference_nss_certificate_functions#cert_checkcertvalidtimes`
      -  :ref:`mozilla_projects_nss_reference_nss_certificate_functions#nss_cmpcertchainwcanames`

   -  :ref:`mozilla_projects_nss_reference_nss_certificate_functions#manipulating_certificates`

      -  :ref:`mozilla_projects_nss_reference_nss_certificate_functions#cert_dupcertificate`
      -  :ref:`mozilla_projects_nss_reference_nss_certificate_functions#cert_destroycertificate`
      -  SEC_DeletePermCertificate
      -  \__CERT_ClosePermCertDB

   -  :ref:`mozilla_projects_nss_reference_nss_certificate_functions#getting_certificate_information`

      -  :ref:`mozilla_projects_nss_reference_nss_certificate_functions#cert_findcertbyname`
      -  :ref:`mozilla_projects_nss_reference_nss_certificate_functions#cert_getcertnicknames`
      -  :ref:`mozilla_projects_nss_reference_nss_certificate_functions#cert_freenicknames`
      -  :ref:`mozilla_projects_nss_reference_nss_certificate_functions#cert_getdefaultcertdb`
      -  :ref:`mozilla_projects_nss_reference_nss_certificate_functions#nss_findcertkeatype`

   -  :ref:`mozilla_projects_nss_reference_nss_certificate_functions#comparing_secitem_objects`

      -  :ref:`mozilla_projects_nss_reference_nss_certificate_functions#secitem_compareitem`

.. _key_functions:

`Key functions <#key_functions>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   :ref:`mozilla_projects_nss_reference_nss_key_functions`

   -  :ref:`mozilla_projects_nss_ssl_functions_sslkey#seckey_getdefaultkeydb`
   -  :ref:`mozilla_projects_nss_ssl_functions_sslkey#seckey_destroyprivatekey`

.. _digital_signatures:

`Digital signatures <#digital_signatures>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   This API consists of the routines used to perform signature generation and the routines used to
   perform signature verification.

.. _encryption.2fdecryption:

`Encryption/decryption <#encryption.2fdecryption>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

`Hashing <#hashing>`__
~~~~~~~~~~~~~~~~~~~~~~

.. container::

.. _key_generation:

`Key generation <#key_generation>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Generate keys, key pairs, and domain parameters.

.. _random_number_generation:

`Random number generation <#random_number_generation>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   This API consists of the two routines used for pseudorandom number generation --
   PK11_GenerateRandomOnSlot and PK11_GenerateRandom -- and the two routines used for seeding
   pseudorandom number generation -- PK11_SeedRandom and PK11_RandomUpdate.

.. _pkcs_.2311_functions:

`PKCS #11 functions <#pkcs_.2311_functions>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Based on :ref:`mozilla_projects_nss_ssl_functions_pkfnc` in the SSL Reference and
   :ref:`mozilla_projects_nss_reference_nss_functions#cryptography_functions` in NSS Public
   Functions.

   -  :ref:`mozilla_projects_nss_pkcs11_functions#secmod_loadusermodule`
   -  :ref:`mozilla_projects_nss_pkcs11_functions#secmod_unloadusermodule`
   -  :ref:`mozilla_projects_nss_pkcs11_functions#secmod_closeuserdb`
   -  :ref:`mozilla_projects_nss_pkcs11_functions#secmod_openuserdb`
   -  :ref:`mozilla_projects_nss_pkcs11_functions#pk11_findcertfromnickname`
   -  :ref:`mozilla_projects_nss_pkcs11_functions#pk11_findkeybyanycert`
   -  :ref:`mozilla_projects_nss_pkcs11_functions#pk11_getslotname`
   -  :ref:`mozilla_projects_nss_pkcs11_functions#pk11_gettokenname`
   -  :ref:`mozilla_projects_nss_pkcs11_functions#pk11_ishw`
   -  :ref:`mozilla_projects_nss_pkcs11_functions#pk11_ispresent`
   -  :ref:`mozilla_projects_nss_pkcs11_functions#pk11_isreadonly`
   -  :ref:`mozilla_projects_nss_pkcs11_functions#pk11_setpasswordfunc`

.. _ssl_functions:

`SSL Functions <#ssl_functions>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Based on :ref:`mozilla_projects_nss_ssl_functions_sslfnc` in the SSL Reference and
   :ref:`mozilla_projects_nss_reference_nss_functions#ssl_functions` and
   :ref:`mozilla_projects_nss_reference_nss_functions#deprecated_ssl_functions` in NSS Public
   Functions.

   -  SSL_ConfigServerSessionIDCache
   -  SSL_ClearSessionCache

.. _s.2fmime:

`S/MIME <#s.2fmime>`__
~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Based on the `S/MIME
   Reference <https://www-archive.mozilla.org/projects/security/pki/nss/ref/smime/>`__ (which only
   has one written chapter) and
   :ref:`mozilla_projects_nss_reference_nss_functions#s_2fmime_functions` in NSS Public Functions.

.. _pkcs_.237_functions:

`PKCS #7 functions <#pkcs_.237_functions>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Based on `"Archived PKCS #7 Functions
   documentation." <https://www-archive.mozilla.org/projects/security/pki/nss/ref/nssfunctions.html#pkcs7>`__

.. _pkcs_.235_functions:

`PKCS #5 functions <#pkcs_.235_functions>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Password-based encryption

   -  SEC_PKCS5GetIV
   -  SEC_PKCS5CreateAlgorithmID
   -  SEC_PKCS5GetCryptoAlgorithm
   -  SEC_PKCS5GetKeyLength
   -  SEC_PKCS5GetPBEAlgorithm
   -  SEC_PKCS5IsAlgorithmPBEAlg

.. _pkcs_.2312_functions:

`PKCS #12 functions <#pkcs_.2312_functions>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Based on `"Archived PKCS #12 Functions
   documentation." <https://www-archive.mozilla.org/projects/security/pki/nss/ref/nssfunctions.html#pkcs12>`__
   Used to exchange data such as private keys and certificates between two parties.

   -  SEC_PKCS12CreateExportContext
   -  SEC_PKCS12CreatePasswordPrivSafe
   -  SEC_PKCS12CreateUnencryptedSafe
   -  SEC_PKCS12AddCertAndKey
   -  SEC_PKCS12AddPasswordIntegrity
   -  SEC_PKCS12EnableCipher
   -  SEC_PKCS12Encode
   -  SEC_PKCS12DestroyExportContext
   -  SEC_PKCS12DecoderStart
   -  SEC_PKCS12DecoderImportBags
   -  SEC_PKCS12DecoderUpdate
   -  SEC_PKCS12DecoderFinish
   -  SEC_PKCS12DecoderValidateBags
   -  SEC_PKCS12DecoderVerify
   -  SEC_PKCS12DecoderGetCerts
   -  SEC_PKCS12DecoderSetTargetTokenCAs
   -  SEC_PKCS12DecoderIterateInit
   -  SEC_PKCS12DecoderIterateNext
   -  SEC_PKCS12IsEncryptionAllowed
   -  SEC_PKCS12SetPreferredCipher

.. _nspr_functions:

`NSPR functions <#nspr_functions>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   A small number of :ref:`mozilla_projects_nss_reference_nspr_functions` are required for using the
   certificate verification and SSL functions in NSS.  These functions are listed in this section.

.. _error_codes:

`Error codes <#error_codes>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Based on :ref:`mozilla_projects_nss_ssl_functions_sslerr` in the SSL Reference.

.. _nss_environment_variables:

`NSS Environment variables <#nss_environment_variables>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   :ref:`mozilla_projects_nss_reference_nss_environment_variables`

.. _nss_cryptographic_module:

`NSS cryptographic module <#nss_cryptographic_module>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   :ref:`mozilla_projects_nss_reference_nss_cryptographic_module`

.. _nss_tech_notes:

`NSS Tech Notes <#nss_tech_notes>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   :ref:`mozilla_projects_nss_nss_tech_notes` :ref:`mozilla_projects_nss_memory_allocation`

`Tools <#tools>`__
~~~~~~~~~~~~~~~~~~

.. container::

   Based on :ref:`mozilla_projects_nss_tools` documentation.

   Based on :ref:`mozilla_projects_nss_tools`