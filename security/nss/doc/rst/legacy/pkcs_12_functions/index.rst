.. _mozilla_projects_nss_pkcs_12_functions:

PKCS 12 functions
=================

.. container::

   The public functions listed here perform PKCS #12 operations required by some of the NSS tools
   and other applications.

   The `Mozilla Cross Reference <http://mxr.mozilla.org/>`__ (MXR) link for each function provides
   access to the function definition, prototype definition, and source code references. The NSS
   version column indicates which versions of NSS support the function.

   ====================================== =========== ==============
   Function name/documentation            Source code NSS versions
   ``SEC_PKCS12AddCertAndKey``            MXR         3.2 and later
   ``SEC_PKCS12AddPasswordIntegrity``     MXR         3.2 and later
   ``SEC_PKCS12CreateExportContext``      MXR         3.2 and later
   ``SEC_PKCS12CreatePasswordPrivSafe``   MXR         3.2 and later
   ``SEC_PKCS12CreateUnencryptedSafe``    MXR         3.2 and later
   ``SEC_PKCS12DecoderFinish``            MXR         3.2 and later
   ``SEC_PKCS12DecoderGetCerts``          MXR         3.4 and later
   ``SEC_PKCS12DecoderImportBags``        MXR         3.2 and later
   ``SEC_PKCS12DecoderIterateInit``       MXR         3.10 and later
   ``SEC_PKCS12DecoderIterateNext``       MXR         3.10 and later
   ``SEC_PKCS12DecoderSetTargetTokenCAs`` MXR         3.8 and later
   ``SEC_PKCS12DecoderStart``             MXR         3.2 and later
   ``SEC_PKCS12DecoderUpdate``            MXR         3.2 and later
   ``SEC_PKCS12DecoderValidateBags``      MXR         3.2 and later
   ``SEC_PKCS12DecoderVerify``            MXR         3.2 and later
   ``SEC_PKCS12DestroyExportContext``     MXR         3.2 and later
   ``SEC_PKCS12EnableCipher``             MXR         3.2 and later
   ``SEC_PKCS12Encode``                   MXR         3.2 and later
   ``SEC_PKCS12IsEncryptionAllowed``      MXR         3.2 and later
   ``SEC_PKCS12SetPreferredCipher``       MXR         3.2 and later
   ====================================== =========== ==============