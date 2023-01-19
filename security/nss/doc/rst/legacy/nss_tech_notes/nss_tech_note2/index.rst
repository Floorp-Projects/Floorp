.. _mozilla_projects_nss_nss_tech_notes_nss_tech_note2:

nss tech note2
==============

.. container::

   .. rubric:: Using the PKCS #11 Module Logger
      :name: Using_the_PKCS_11_Module_Logger

.. _nss_technical_note_2:

`NSS Technical Note: 2 <#nss_technical_note_2>`__
-------------------------------------------------

.. container::

   -  `Modes of Operation <#modes>`__
   -  `Extracting Output from Log files <#extracting>`__

   The logger displays all activity between NSS and a specified PKCS #11 module. It works by
   inserting a special set of entry points between NSS and the module.

   To enable the module logger, you must set the environment variable NSS_DEBUG_PKCS11_MODULE to the
   name of the target module. For example, to log the softoken, use:

   .. code::

      NSS_DEBUG_PKCS11_MODULE="NSS Internal PKCS #11 Module"

   Note: In the Command Prompt on Windows, do not quote the name of the target module, otherwise the
   quotes are considered part of the name. For example, to log the softoken on Windows, use:

   .. code::

            set NSS_DEBUG_PKCS11_MODULE=NSS Internal PKCS #11 Module

   The logger is available by default in debug builds. For optimized builds, NSS must be built with
   the variable DEBUG_PKCS11 set.

.. _modes_of_operation:

`Modes of Operation <#modes_of_operation>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   The logger has several modes of operation:

   **1. Only display the sequence of PKCS #11 calls.** To enable this mode, set:

   .. code::

      NSPR_LOG_MODULES=nss_mod_log:1
      NSPR_LOG_FILE=<logfile>

   The output format is:

   .. code::

      OSThreadID[NSPRThreadID]: C_XXX
      OSThreadID[NSPRThreadID]:   rv = 0xYYYYYYYY

   For example,

   .. code::

      1024[805ef10]: C_Initialize
      1024[805ef10]:   rv = 0x0
      1024[805ef10]: C_GetInfo
      1024[805ef10]:   rv = 0x0
      1024[805ef10]: C_GetSlotList
      1024[805ef10]:   rv = 0x0

   **2. Display the sequence of PKCS #11 calls, and the parameters given to them.** To enable this
   mode, set:

   .. code::

      NSPR_LOG_MODULES=nss_mod_log:3
      NSPR_LOG_FILE=<logfile>

   The output format is:

   .. code::

      OSThreadID[NSPRThreadID]: C_XXX
      OSThreadID[NSPRThreadID]:   arg1 = 0xAAAAAAAA
      ...
      OSThreadID[NSPRThreadID]:   argN = 0xAAAAAAAA
      OSThreadID[NSPRThreadID]:   rv = 0xYYYYYYYY

   For example,

   .. code::

      1024[805ef10]: C_Initialize
      1024[805ef10]:   pInitArgs = 0x4010c938
      1024[805ef10]:   rv = 0x0
      1024[805ef10]: C_GetInfo
      1024[805ef10]:   pInfo = 0xbffff340
      1024[805ef10]:   rv = 0x0
      1024[805ef10]: C_GetSlotList
      1024[805ef10]:   tokenPresent = 0x0
      1024[805ef10]:   pSlotList = 0x0
      1024[805ef10]:   pulCount = 0xbffff33c
      1024[805ef10]:   *pulCount = 0x2
      1024[805ef10]:   rv = 0x0

   Note that when a PKCS #11 function takes a pointer argument for which it will set a value
   (C_GetSlotList above), this mode will display the value upon return.

   **3. Display verbose information, including template values, array values, etc.** To enable this
   mode, set:

   .. code::

      NSPR_LOG_MODULES=nss_mod_log:4
      NSPR_LOG_FILE=<logfile>

   The output format is the same as above, but with more information. For example,

   .. code::

      1024[805ef10]: C_FindObjectsInit
      1024[805ef10]:   hSession = 0x1000001
      1024[805ef10]:   pTemplate = 0xbffff410
      1024[805ef10]:   ulCount = 3
      1024[805ef10]:     CKA_LABEL = localhost.nyc.rr.com [20]
      1024[805ef10]:     CKA_TOKEN = CK_TRUE [1]
      1024[805ef10]:     CKA_CLASS = CKO_CERTIFICATE [4]
      1024[805ef10]:   rv = 0x0
      1024[805ef10]: C_FindObjects
      1024[805ef10]:   hSession = 0x1000001
      1024[805ef10]:   phObject = 0x806d810
      1024[805ef10]:   ulMaxObjectCount = 16
      1024[805ef10]:   pulObjectCount = 0xbffff38c
      1024[805ef10]:   *pulObjectCount = 0x1
      1024[805ef10]:   phObject[0] = 0xf6457d04
      1024[805ef10]:   rv = 0x0
      1024[805ef10]: C_FindObjectsFinal
      1024[805ef10]:   hSession = 0x1000001
      1024[805ef10]:   rv = 0x0
      1024[805ef10]: C_GetAttributeValue
      1024[805ef10]:   hSession = 0x1000001
      1024[805ef10]:   hObject = 0xf6457d04
      1024[805ef10]:   pTemplate = 0xbffff2d0
      1024[805ef10]:   ulCount = 2
      1024[805ef10]:     CKA_TOKEN = 0 [1]
      1024[805ef10]:     CKA_LABEL = 0 [20]
      1024[805ef10]:   rv = 0x0
      1024[805ef10]: C_GetAttributeValue
      1024[805ef10]:   hSession = 0x1000001
      1024[805ef10]:   hObject = 0xf6457d04
      1024[805ef10]:   pTemplate = 0xbffff2d0
      1024[805ef10]:   ulCount = 2
      1024[805ef10]:     CKA_TOKEN = CK_TRUE [1]
      1024[805ef10]:     CKA_LABEL = localhost.nyc.rr.com [20]
      1024[805ef10]:   rv = 0x0

   **4. Collect performance data.** This mode is most useful in optimized builds. The number of
   calls to each PKCS #11 function will be counted, and the time spent in each function as well. A
   summary of performance data is dumped during NSS shutdown.

   No additional environment variables are required for this mode. If the environment variable
   NSS_OUTPUT_FILE is set, its value will be used as the path name of the file to which the final
   output will be written. Otherwise, the output will be written to stdout.