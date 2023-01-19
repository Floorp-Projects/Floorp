.. _mozilla_projects_nss_reference_nss_initialize:

NSS_Initialize
==============

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   NSS_Initialize - initialize NSS.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code::

      SECStatus NSS_Initialize(const char *configdir,
                               const char *certPrefix,
                               const char *keyPrefix,
                               const char *secmodName,
                               PRUint32 flags);

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``NSS_Initialize`` has five parameters:

   ``configdir``
      [in] the directory where the certificate, key, and module databases live. To-do: document the
      "sql:" prefix.
   ``certPrefix``
      [in] prefix added to the beginning of the certificate database, for example, "https-server1-".
   ``keyPrefix``
      [in] prefix added to the beginning of the key database, for example, "https-server1-".
   ``secmodName``
      [in] name of the security module database, usually "secmod.db".
   ``flags``
      [in] bit flags that specify how NSS should be initialized.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``NSS_Initialize`` initializes NSS. It is more flexible than ``NSS_Init``, ``NSS_InitReadWrite``,
   and ``NSS_NoDB_Init``. If any of those simpler NSS initialization functions suffices for your
   needs, call that instead.

   The ``flags`` parameter is a bitwise OR of the following flags:

   -  NSS_INIT_READONLY - Open the databases read only.
   -  NSS_INIT_NOCERTDB - Don't open the cert DB and key DB's, just initialize the volatile certdb.
   -  NSS_INIT_NOMODDB - Don't open the security module DB, just initialize the  PKCS #11 module.
   -  NSS_INIT_FORCEOPEN - Continue to force initializations even if the databases cannot be opened.
   -  NSS_INIT_NOROOTINIT - Don't try to look for the root certs module automatically.
   -  NSS_INIT_OPTIMIZESPACE - Optimize for space instead of speed. Use smaller tables and caches.
   -  NSS_INIT_PK11THREADSAFE - only load PKCS#11 modules that are thread-safe, i.e., that support
      locking - either OS locking or NSS-provided locks . If a PKCS#11 module isn't thread-safe,
      don't serialize its calls; just don't load it instead. This is necessary if another piece of
      code is using the same PKCS#11 modules that NSS is accessing without going through NSS, for
      example, the Java SunPKCS11 provider.
   -  NSS_INIT_PK11RELOAD - ignore the CKR_CRYPTOKI_ALREADY_INITIALIZED error when loading PKCS#11
      modules. This is necessary if another piece of code is using the same PKCS#11 modules that NSS
      is accessing without going through NSS, for example, Java SunPKCS11 provider.
   -  NSS_INIT_NOPK11FINALIZE - never call C_Finalize on any PKCS#11 module. This may be necessary
      in order to ensure continuous operation and proper shutdown sequence if another piece of code
      is using the same PKCS#11 modules that NSS is accessing without going through NSS, for
      example, Java SunPKCS11 provider. The following limitation applies when this is set
      : SECMOD_WaitForAnyTokenEvent will not use C_WaitForSlotEvent, in order to prevent the need
      for C_Finalize. This call will be emulated instead.
   -  NSS_INIT_RESERVED - Currently has no effect, but may be used in the future to trigger better
      cooperation between PKCS#11 modules used by both NSS and the Java SunPKCS11 provider. This
      should occur after a new flag is defined for C_Initialize by the PKCS#11 working group.
   -  NSS_INIT_COOPERATE - Sets the above four recommended options for applications that use both
      NSS and the Java SunPKCS11 provider.

.. _return_value:

`Return value <#return_value>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``NSS_Initialize`` returns SECSuccess on success, or SECFailure on failure.

`Examples <#examples>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code::

      #include <nss.h>

      SECStatus rv;
      const char *configdir;

      configdir = ...;  /* application-specific */
      rv = NSS_Initialize(configdir, "", "", SECMOD_DB, NSS_INIT_NOROOTINIT | NSS_INIT_OPTIMIZESPACE);

.. _see_also:

`See also <#see_also>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  NSS_Init, NSS_InitReadWrite, NSS_NoDB_Init, NSS_Shutdown