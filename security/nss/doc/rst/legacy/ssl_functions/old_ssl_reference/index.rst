.. _mozilla_projects_nss_ssl_functions_old_ssl_reference:

OLD SSL Reference
=================

.. container::

   .. rubric:: OLD SSL Reference
      :name: OLD_SSL_Reference

   .. note::

      -  We are migrating this SSL Reference into the format described in the `MDN Style
         Guide <https://developer.mozilla.org/en-US/docs/Project:MDC_style_guide>`__. If you are
         inclined to help with this migration, your help would be very much appreciated.

      -  Upgraded documentation may be found in the :ref:`mozilla_projects_nss_reference`

.. _ssl_reference:

`SSL Reference <#ssl_reference>`__
----------------------------------

.. container::

   *Newsgroup:*\ `mozilla.dev.tech.crypto <news://news.mozilla.org/mozilla.dev.tech.crypto>`__\ *
   Writer: Sean Cotter
   Manager: Wan-Teh Chang*

   .. rubric:: `Chapter 1  Overview of an SSL Application <sslintro.html#1028068>`__
      :name: chapter_1_overview_of_an_ssl_application

      SSL and related APIs allow compliant applications to configure sockets for authenticated,
      tamper-proof, and encrypted communications. This chapter introduces some of the basic SSL
      functions. Chapter 2, "Getting Started With SSL" illustrates their use in sample client and
      server applications.

   -  `Initialization <sslintro.html#1027662>`__

      -  `Initializing Caches <sslintro.html#1039943>`__

   -  `Configuration <sslintro.html#1027742>`__ ` <sslintro.html#1027816>`__
   -  `Communication <sslintro.html#1027816>`__ ` <sslintro.html#1027820>`__
   -  `Functions Used by Callbacks <sslintro.html#1027820>`__ ` <sslintro.html#1030535>`__
   -  `Cleanup <sslintro.html#1030535>`__

   .. rubric:: `Chapter 2  Getting Started With SSL <gtstd.html#1005439>`__
      :name: chapter_2_getting_started_with_ssl

      This chapter describes how to set up your environment, including certificate and key
      databases, to run the NSS sample code. The sample code and makefiles are available via LXR in
      the SSLSamples directory.

   -  `SSL, PKCS #11, and the Default Security Databases <gtstd.html#1011970>`__
      ` <gtstd.html#1011987>`__
   -  `Setting Up the Certificate and Key Databases <gtstd.html#1011987>`__

      -  `Setting Up the CA DB and Certificate <gtstd.html#1012301>`__ ` <gtstd.html#1012351>`__
      -  `Setting Up the Server DB and Certificate <gtstd.html#1012351>`__ ` <gtstd.html#1012067>`__
      -  `Setting Up the Client DB and Certificate <gtstd.html#1012067>`__ ` <gtstd.html#1012108>`__
      -  `Verifying the Server and Client Certificates <gtstd.html#1012108>`__

   -  `Building NSS Programs <gtstd.html#1013274>`__

   .. rubric:: `Chapter 3  Selected SSL Types and Structures <ssltyp.html#1029792>`__
      :name: chapter_3_selected_ssl_types_and_structures

      This chapter describes some of the most important types and structures used with the functions
      described in the rest of this document, and how to manage the memory used for them. Additional
      types are described with the functions that use them or in the header files.

   -  `Types and Structures <ssltyp.html#1030559>`__

      -  `CERTCertDBHandle <ssltyp.html#1028465>`__ ` <ssltyp.html#1027387>`__
      -  `CERTCertificate <ssltyp.html#1027387>`__ ` <ssltyp.html#1028593>`__
      -  `PK11SlotInfo <ssltyp.html#1028593>`__ ` <ssltyp.html#1026076>`__
      -  `SECItem <ssltyp.html#1026076>`__ ` <ssltyp.html#1026727>`__
      -  `SECKEYPrivateKey <ssltyp.html#1026727>`__ ` <ssltyp.html#1026722>`__
      -  `SECStatus <ssltyp.html#1026722>`__

   -  `Managing SECItem Memory <ssltyp.html#1029645>`__

      -  `SECItem_FreeItem <ssltyp.html#1030620>`__ ` <ssltyp.html#1030773>`__
      -  `SECItem_ZfreeItem <ssltyp.html#1030773>`__

   .. rubric:: :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1047959`
      :name: chapter_4_ssl_functions

      This chapter describes the core SSL functions.

   -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1022864`

      -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1067601`
         :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1237143`
      -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1237143`
         :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1234224`
      -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1234224`
         :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1068466`
      -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1068466`
         :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1204897`
      -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1204897`
         :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1084747`
      -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1084747`
         :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1208119`
      -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1208119`
         :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1138601`
      -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1138601`
         :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1143851`
      -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1143851`

   -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1154189`

      -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1142625`
         :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1162055`
      -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1162055`

   -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1098841`

      -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1228530`
         :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1100285`
      -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1100285`
         :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1105952`
      -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1105952`
         :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1104647`
      -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1104647`
         :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1210463`
      -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1210463`

   -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1163855`

      -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1090577`

         -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1085950`
            :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1086543`
         -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1086543`
            :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1194921`
         -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1194921`
            :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1214758`
         -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1214758`
            :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1214800`
         -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1214800`
            :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1217647`
         -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1217647`
            :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1087792`
         -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1087792`
            :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1088040`
         -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1088040`

      -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1089578`

         -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1088805`
            :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1088888`
         -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1088888`
            :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1088928`
         -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1088928`
            :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1126622`
         -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1126622`
            :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1106762`
         -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1106762`
            :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1112702`
         -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1112702`

   -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1127321`

      -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1089420`
         :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1092785`
      -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1092785`
         :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1092805`
      -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1092805`
         :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1092869`
      -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1092869`
         :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1124562`
      -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1124562`

   -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1127893`

      -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1096168`
         :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1081175`
      -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1081175`
         :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1123385`
      -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1123385`

   -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1061582`

      -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1133431`
         :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1232052`
      -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1232052`
         :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1058001`
      -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1058001`

   -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1095840`

      -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1061858`

   -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1198429`

      -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1206365`
         :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1220189`
      -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1220189`
         :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1207298`
      -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1207298`
         :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1207350`
      -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1207350`
         :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1231825`
      -  :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1231825`

   .. rubric:: `Chapter 5  Certificate Functions <sslcrt.html#1047959>`__
      :name: chapter_5_certificate_functions

      This chapter describes the functions and related types used to work with a certificate
      database such as the cert7.db database provided with Communicator.

   -  `Validating Certificates <sslcrt.html#1060423>`__

      -  `CERT_VerifyCertNow <sslcrt.html#1058011>`__ ` <sslcrt.html#1050342>`__
      -  `CERT_VerifyCertName <sslcrt.html#1050342>`__ ` <sslcrt.html#1056662>`__
      -  `CERT_CheckCertValidTimes <sslcrt.html#1056662>`__ ` <sslcrt.html#1056760>`__
      -  `NSS_CmpCertChainWCANames <sslcrt.html#1056760>`__

   -  `Manipulating Certificates <sslcrt.html#1056436>`__

      -  `CERT_DupCertificate <sslcrt.html#1058344>`__ ` <sslcrt.html#1050532>`__
      -  `CERT_DestroyCertificate <sslcrt.html#1050532>`__

   -  `Getting Certificate Information <sslcrt.html#1056475>`__

      -  `CERT_FindCertByName <sslcrt.html#1050345>`__ ` <sslcrt.html#1050346>`__
      -  `CERT_GetCertNicknames <sslcrt.html#1050346>`__ ` <sslcrt.html#1050349>`__
      -  `CERT_FreeNicknames <sslcrt.html#1050349>`__ ` <sslcrt.html#1052308>`__
      -  `CERT_GetDefaultCertDB <sslcrt.html#1052308>`__ ` <sslcrt.html#1056950>`__
      -  `NSS_FindCertKEAType <sslcrt.html#1056950>`__

   -  `Comparing SecItem Objects <sslcrt.html#1055384>`__

      -  `SECITEM_CompareItem <sslcrt.html#1057028>`__

   .. rubric:: `Chapter 6  Key Functions <sslkey.html#1047959>`__
      :name: chapter_6_key_functions

      This chapter describes two functions used to manipulate private keys and key databases such as
      the key3.db database provided with Communicator.

   -  `SECKEY_GetDefaultKeyDB <sslkey.html#1051479>`__ ` <sslkey.html#1051017>`__
   -  `SECKEY_DestroyPrivateKey <sslkey.html#1051017>`__

   .. rubric:: `Chapter 7  PKCS #11 Functions <pkfnc.html#1027946>`__
      :name: chapter_7_pkcs_11_functions

      This chapter describes the core PKCS #11 functions that an application needs for communicating
      with cryptographic modules. In particular, these functions are used for obtaining
      certificates, keys, and passwords.

   -  `PK11_FindCertFromNickname <pkfnc.html#1035673>`__ ` <pkfnc.html#1026891>`__
   -  `PK11_FindKeyByAnyCert <pkfnc.html#1026891>`__ ` <pkfnc.html#1030779>`__
   -  `PK11_GetSlotName <pkfnc.html#1030779>`__ ` <pkfnc.html#1026964>`__
   -  `PK11_GetTokenName <pkfnc.html#1026964>`__ ` <pkfnc.html#1026762>`__
   -  `PK11_IsHW <pkfnc.html#1026762>`__ ` <pkfnc.html#1022948>`__
   -  `PK11_IsPresent <pkfnc.html#1022948>`__ ` <pkfnc.html#1022991>`__
   -  `PK11_IsReadOnly <pkfnc.html#1022991>`__ ` <pkfnc.html#1023128>`__
   -  `PK11_SetPasswordFunc <pkfnc.html#1023128>`__

   .. rubric:: `Chapter 8  NSS and SSL Error Codes <sslerr.html#1013897>`__
      :name: chapter_8_nss_and_ssl_error_codes

      NSS error codes are retrieved using the NSPR function PR_GetError. In addition to the error
      codes defined by NSPR, PR_GetError retrieves the error codes described in this chapter.

   -  `SSL Error Codes <sslerr.html#1040263>`__ ` <sslerr.html#1039257>`__
   -  `SEC Error Codes <sslerr.html#1039257>`__