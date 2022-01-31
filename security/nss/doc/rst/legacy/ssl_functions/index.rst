.. _mozilla_projects_nss_ssl_functions:

SSL functions
=============

.. container::

   The public functions listed here are used to configure sockets for communication via the SSL and
   TLS protocols. In addition to the functions listed here, applications that support SSL use some
   of the Certificate functions, Crypto functions, and Utility functions described below on this
   page.

   Other sources of information:

   -  The :ref:`mozilla_projects_nss_reference` documents the functions most commonly used by
      applications to support SSL.
   -  The :ref:`mozilla_projects_nss` home page links to additional SSL documentation.

   If documentation is available for a function listed below, the function name is linked to either
   its MDC wiki page or its entry in the
   :ref:`mozilla_projects_nss_ssl_functions_old_ssl_reference`. The `Mozilla Cross
   Reference <https://dxr.mozilla.org/>`__ (DXR) link for each function provides access to the
   function definition, prototype definition, and source code references. The NSS version column
   indicates which versions of NSS support the function.

   ======================================================== =========== ================
   Function name/documentation                              Source code NSS versions
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1106762` MXR         3.2 and later
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1228530` MXR         3.2 and later
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1100285` MXR         3.2 and later
   ``NSS_SetFrancePolicy``                                  MXR         3.2 and later
   ``NSSSSL_VersionCheck``                                  MXR         3.2.1 and later
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1088888` MXR         3.2 and later
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1088805` MXR         3.2 and later
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1088928` MXR         3.2 and later
   ``SSL_CertDBHandleSet``                                  MXR         3.2 and later
   ``SSL_Canbypass``                                        MXR         3.11.7 and later
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1210463` MXR         3.2 and later
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1104647` MXR         3.2 and later
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1214800` MXR         3.2 and later
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1208119` MXR         3.2 and later
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1214758` MXR         3.2 and later
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1084747` MXR         3.2 and later
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1138601` MXR         3.2 and later
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1142625` MXR         3.2 and later
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1217647` MXR         3.2 and later
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1143851` MXR         3.2 and later
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1142625` MXR         3.2 and later
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1133431` MXR         3.2 and later
   ``SSL_ForceHandshakeWithTimeout``                        MXR         3.11.4 and later
   ``SSL_GetChannelInfo``                                   MXR         3.4 and later
   ``SSL_GetCipherSuiteInfo``                               MXR         3.4 and later
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1126622` MXR         3.2 and later
   ``SSL_GetMaxServerCacheLocks``                           MXR         3.4 and later
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1092869` MXR         3.2 and later
   ``SSL_GetStatistics``                                    MXR         3.2 and later
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1112702` MXR         3.2 and later
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1085950` MXR         3.2 and later
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1162055` MXR         3.2 and later
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1089420` MXR         3.2 and later
   ``SSL_LocalCertificate``                                 MXR         3.4 and later
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1194921` MXR         3.2 and later
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1204897` MXR         3.2 and later
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1086543` MXR         3.2 and later
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1068466` MXR         3.2 and later
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1096168` MXR         3.2 and later
   ``SSL_PreencryptedFileToStream``                         MXR         3.2 and later
   ``SSL_PreencryptedStreamToFile``                         MXR         3.2 and later
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1232052` MXR         3.2 and later
   ``SSL_ReHandshakeWithTimeout``                           MXR         3.11.4 and later
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1058001` MXR         3.2 and later
   ``SSL_RestartHandshakeAfterCertReq``                     MXR         3.2 and later
   ``SSL_RestartHandshakeAfterServerCert``                  MXR         3.2 and later
   ``SSL_RevealCert``                                       MXR         3.2 and later
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1123385` MXR         3.2 and later
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1081175` MXR         3.2 and later
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1092805` MXR         3.2 and later
   ``SSL_SetMaxServerCacheLocks``                           MXR         3.4 and later
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1088040` MXR         3.2 and later
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1124562` MXR         3.2 and later
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1087792` MXR         3.2 and later
   ``SSL_ShutdownServerSessionIDCache``                     MXR         3.7.4 and later
   ======================================================== =========== ================