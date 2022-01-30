.. _mozilla_projects_nss_pkcs_7_functions:

PKCS 7 functions
================

.. container::

   The public functions listed here perform PKCS #7 operations required by mail and news
   applications and by some of the NSS tools.

   The `Mozilla Cross Reference <http://mxr.mozilla.org/>`__ (MXR) link for each function provides
   access to the function definition, prototype definition, and source code references. The NSS
   version column indicates which versions of NSS support the function.

   ==================================== =========== ===============
   Function name/documentation          Source code NSS versions
   ``SEC_PKCS7AddCertificate``          MXR         3.3 and later
   ``SEC_PKCS7AddRecipient``            MXR         3.2 and later
   ``SEC_PKCS7AddSigningTime``          MXR         3.2 and later
   ``SEC_PKCS7ContainsCertsOrCrls``     MXR         3.4 and later
   ``SEC_PKCS7ContentIsEncrypted``      MXR         3.4 and later
   ``SEC_PKCS7ContentIsSigned``         MXR         3.4 and later
   ``SEC_PKCS7ContentType``             MXR         3.2 and later
   ``SEC_PKCS7CopyContentInfo``         MXR         3.4 and later
   ``SEC_PKCS7CreateCertsOnly``         MXR         3.3 and later
   ``SEC_PKCS7CreateData``              MXR         3.2 and later
   ``SEC_PKCS7CreateEncryptedData``     MXR         3.2 and later
   ``SEC_PKCS7CreateEnvelopedData``     MXR         3.2 and later
   ``SEC_PKCS7CreateSignedData``        MXR         3.2 and later
   ``SEC_PKCS7DecodeItem``              MXR         3.2 and later
   ``SEC_PKCS7DecoderAbort``            MXR         3.9 and later
   ``SEC_PKCS7DecoderFinish``           MXR         3.2 and later
   ``SEC_PKCS7DecoderStart``            MXR         3.2 and later
   ``SEC_PKCS7DecoderUpdate``           MXR         3.2 and later
   ``SEC_PKCS7DecryptContents``         MXR         3.2 and later
   ``SEC_PKCS7DestroyContentInfo``      MXR         3.2 and later
   ``SEC_PKCS7Encode``                  MXR         3.3 and later
   ``SEC_PKCS7EncodeItem``              MXR         3.9.3 and later
   ``SEC_PKCS7EncoderAbort``            MXR         3.9 and later
   ``SEC_PKCS7EncoderFinish``           MXR         3.2 and later
   ``SEC_PKCS7EncoderStart``            MXR         3.2 and later
   ``SEC_PKCS7EncoderUpdate``           MXR         3.2 and later
   ``SEC_PKCS7GetCertificateList``      MXR         3.2 and later
   ``SEC_PKCS7GetContent``              MXR         3.2 and later
   ``SEC_PKCS7GetEncryptionAlgorithm``  MXR         3.2 and later
   ``SEC_PKCS7GetSignerCommonName``     MXR         3.4 and later
   ``SEC_PKCS7GetSignerEmailAddress``   MXR         3.4 and later
   ``SEC_PKCS7GetSigningTime``          MXR         3.4 and later
   ``SEC_PKCS7IncludeCertChain``        MXR         3.2 and later
   ``SEC_PKCS7IsContentEmpty``          MXR         3.2 and later
   ``SEC_PKCS7SetContent``              MXR         3.4 and later
   ``SEC_PKCS7VerifyDetachedSignature`` MXR         3.4 and later
   ``SEC_PKCS7VerifySignature``         MXR         3.2 and later
   ``SECMIME_DecryptionAllowed``        MXR         3.4 and later
   ==================================== =========== ===============