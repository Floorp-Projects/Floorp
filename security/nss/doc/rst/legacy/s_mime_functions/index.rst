.. _mozilla_projects_nss_s_mime_functions:

S/MIME functions
================

.. container::

   The public functions listed here perform S/MIME operations using the `S/MIME
   Toolkit <http://www-archive.mozilla.org/projects/security/pki/nss/smime/>`__.

   The `Mozilla Cross Reference <http://mxr.mozilla.org/>`__ (MXR) link for each function provides
   access to the function definition, prototype definition, and source code references. The NSS
   version column indicates which versions of NSS support the function.

   ==================================================== =========== ===============
   Function name/documentation                          Source code NSS versions
   ``NSS_CMSContentInfo_GetBulkKey``                    MXR         3.2 and later
   ``NSS_CMSContentInfo_GetBulkKeySize``                MXR         3.2 and later
   ``NSS_CMSContentInfo_GetContent``                    MXR         3.2 and later
   ``NSS_CMSContentInfo_GetContentEncAlgTag``           MXR         3.2 and later
   ``NSS_CMSContentInfo_GetContentTypeTag``             MXR         3.2 and later
   ``NSS_CMSContentInfo_SetBulkKey``                    MXR         3.2 and later
   ``NSS_CMSContentInfo_SetContent``                    MXR         3.2 and later
   ``NSS_CMSContentInfo_SetContent_Data``               MXR         3.2 and later
   ``NSS_CMSContentInfo_SetContentEncAlg``              MXR         3.2 and later
   ``NSS_CMSContentInfo_SetContent_DigestedData``       MXR         3.2 and later
   ``NSS_CMSContentInfo_SetContent_EncryptedData``      MXR         3.2 and later
   ``NSS_CMSContentInfo_SetContent_EnvelopedData``      MXR         3.2 and later
   ``NSS_CMSContentInfo_SetContent_SignedData``         MXR         3.2 and later
   ``NSS_CMSDecoder_Cancel``                            MXR         3.2 and later
   ``NSS_CMSDecoder_Finish``                            MXR         3.2 and later
   ``NSS_CMSDecoder_Start``                             MXR         3.2 and later
   ``NSS_CMSDecoder_Update``                            MXR         3.2 and later
   ``NSS_CMSDigestContext_Cancel``                      MXR         3.2 and later
   ``NSS_CMSDigestContext_FinishMultiple``              MXR         3.2 and later
   ``NSS_CMSDigestContext_FinishSingle``                MXR         3.2 and later
   ``NSS_CMSDigestContext_StartMultiple``               MXR         3.2 and later
   ``NSS_CMSDigestContext_StartSingle``                 MXR         3.2 and later
   ``NSS_CMSDigestContext_Update``                      MXR         3.2 and later
   ``NSS_CMSDigestedData_Create``                       MXR         3.2 and later
   ``NSS_CMSDigestedData_Destroy``                      MXR         3.2 and later
   ``NSS_CMSDigestedData_GetContentInfo``               MXR         3.2 and later
   ``NSS_CMSDEREncode``                                 MXR         3.2 and later
   ``NSS_CMSEncoder_Cancel``                            MXR         3.2 and later
   ``NSS_CMSEncoder_Finish``                            MXR         3.2 and later
   ``NSS_CMSEncoder_Start``                             MXR         3.2 and later
   ``NSS_CMSEncoder_Update``                            MXR         3.2 and later
   ``NSS_CMSEncryptedData_Create``                      MXR         3.2 and later
   ``NSS_CMSEncryptedData_Destroy``                     MXR         3.2 and later
   ``NSS_CMSEncryptedData_GetContentInfo``              MXR         3.2 and later
   ``NSS_CMSEnvelopedData_AddRecipient``                MXR         3.2 and later
   ``NSS_CMSEnvelopedData_Create``                      MXR         3.2 and later
   ``NSS_CMSEnvelopedData_Destroy``                     MXR         3.2 and later
   ``NSS_CMSEnvelopedData_GetContentInfo``              MXR         3.2 and later
   ``NSS_CMSMessage_ContentLevel``                      MXR         3.2 and later
   ``NSS_CMSMessage_ContentLevelCount``                 MXR         3.2 and later
   ``NSS_CMSMessage_Copy``                              MXR         3.2 and later
   ``NSS_CMSMessage_Create``                            MXR         3.2 and later
   ``NSS_CMSMessage_CreateFromDER``                     MXR         3.2 and later
   ``NSS_CMSMessage_Destroy``                           MXR         3.2 and later
   ``NSS_CMSMessage_GetContent``                        MXR         3.2 and later
   ``NSS_CMSMessage_GetContentInfo``                    MXR         3.2 and later
   ``NSS_CMSMessage_IsEncrypted``                       MXR         3.4.1 and later
   ``NSS_CMSMessage_IsSigned``                          MXR         3.4 and later
   ``NSS_CMSRecipientInfo_Create``                      MXR         3.2 and later
   ``NSS_CMSRecipientInfo_CreateFromDER``               MXR         3.8 and later
   ``NSS_CMSRecipientInfo_CreateNew``                   MXR         3.8 and later
   ``NSS_CMSRecipientInfo_CreateWithSubjKeyID``         MXR         3.7 and later
   ``NSS_CMSRecipientInfo_CreateWithSubjKeyIDFromCert`` MXR         3.7 and later
   ``NSS_CMSRecipientInfo_Destroy``                     MXR         3.2 and later
   ``NSS_CMSRecipientInfo_Encode``                      MXR         3.8 and later
   ``NSS_CMSRecipientInfo_GetCertAndKey``               MXR         3.8 and later
   ``NSS_CMSRecipientInfo_UnwrapBulkKey``               MXR         3.7.2 and later
   ``NSS_CMSRecipientInfo_WrapBulkKey``                 MXR         3.7.2 and later
   ``NSS_CMSSignedData_AddCertChain``                   MXR         3.2 and later
   ``NSS_CMSSignedData_AddCertList``                    MXR         3.2 and later
   ``NSS_CMSSignedData_AddCertificate``                 MXR         3.2 and later
   ``NSS_CMSSignedData_AddDigest``                      MXR         3.2 and later
   ``NSS_CMSSignedData_AddSignerInfo``                  MXR         3.2 and later
   ``NSS_CMSSignedData_Create``                         MXR         3.2 and later
   ``NSS_CMSSignedData_CreateCertsOnly``                MXR         3.2 and later
   ``NSS_CMSSignedData_Destroy``                        MXR         3.2 and later
   ``NSS_CMSSignedData_GetContentInfo``                 MXR         3.2 and later
   ``NSS_CMSSignedData_GetDigestAlgs``                  MXR         3.2 and later
   ``NSS_CMSSignedData_GetSignerInfo``                  MXR         3.2 and later
   ``NSS_CMSSignedData_HasDigests``                     MXR         3.2 and later
   ``NSS_CMSSignedData_ImportCerts``                    MXR         3.2 and later
   ``NSS_CMSSignedData_SetDigests``                     MXR         3.2 and later
   ``NSS_CMSSignedData_SetDigestValue``                 MXR         3.4 and later
   ``NSS_CMSSignedData_SignerInfoCount``                MXR         3.2 and later
   ``NSS_CMSSignedData_VerifyCertsOnly``                MXR         3.2 and later
   ``NSS_CMSSignedData_VerifySignerInfo``               MXR         3.2 and later
   ``NSS_CMSSignerInfo_AddMSSMIMEEncKeyPrefs``          MXR         3.6 and later
   ``NSS_CMSSignerInfo_AddSMIMECaps``                   MXR         3.2 and later
   ``NSS_CMSSignerInfo_AddSMIMEEncKeyPrefs``            MXR         3.2 and later
   ``NSS_CMSSignerInfo_AddSigningTime``                 MXR         3.2 and later
   ``NSS_CMSSignerInfo_Create``                         MXR         3.2 and later
   ``NSS_CMSSignerInfo_CreateWithSubjKeyID``            MXR         3.6 and later
   ``NSS_CMSSignerInfo_Destroy``                        MXR         3.2 and later
   ``NSS_CMSSignerInfo_GetCertList``                    MXR         3.2 and later
   ``NSS_CMSSignerInfo_GetSignerCommonName``            MXR         3.2 and later
   ``NSS_CMSSignerInfo_GetSignerEmailAddress``          MXR         3.2 and later
   ``NSS_CMSSignerInfo_GetSigningCertificate``          MXR         3.2 and later
   ``NSS_CMSSignerInfo_GetSigningTime``                 MXR         3.2 and later
   ``NSS_CMSSignerInfo_GetVerificationStatus``          MXR         3.2 and later
   ``NSS_CMSSignerInfo_GetVersion``                     MXR         3.2 and later
   ``NSS_CMSSignerInfo_IncludeCerts``                   MXR         3.2 and later
   ``NSS_CMSUtil_VerificationStatusToString``           MXR         3.2 and later
   ``NSS_SMIMESignerInfo_SaveSMIMEProfile``             MXR         3.4 and later
   ``NSS_SMIMEUtil_FindBulkAlgForRecipients``           MXR         3.2 and later
   ==================================================== =========== ===============