This chapter describes the NSPR API for versioning, process
initialization, and shutdown of NSPR.

-  `Identity and Versioning <#Identity_and_Versioning>`__
-  `Initialization and Cleanup <#Initialization_and_Cleanup>`__
-  `Module Initialization <#Module_Initialization>`__

.. _Identity_and_Versioning:

Identity and Versioning
-----------------------

.. _Name_and_Version_Constants:

Name and Version Constants
~~~~~~~~~~~~~~~~~~~~~~~~~~

-  ``PR_NAME``
-  ``PR_VERSION``
-  ``PR_VersionCheck``

.. _Initialization_and_Cleanup:

Initialization and Cleanup
--------------------------

NSPR detects whether the library has been initialized and performs
implicit initialization if it hasn't. Implicit initialization should
suffice unless a program has specific sequencing requirements or needs
to characterize the primordial thread. Explicit initialization is rarely
necessary.

Implicit initialization assumes that the initiator is the primordial
thread and that the thread is a user thread of normal priority.

-  ``PR_Init``
-  ``PR_Initialize``
-  ``PR_Initialized``
-  ``PR_Cleanup``
-  ``PR_DisableClockInterrupts``
-  ``PR_BlockClockInterrupts``
-  ``PR_UnblockClockInterrupts``
-  ``PR_SetConcurrency``
-  ``PR_ProcessExit``
-  ``PR_Abort``

.. _Module_Initialization:

Module Initialization
~~~~~~~~~~~~~~~~~~~~~

Initialization can be tricky in a threaded environment, especially
initialization that must happen exactly once. ``PR_CallOnce`` ensures
that such initialization code is called only once. This facility is
recommended in situations where complicated global initialization is
required.

-  ``PRCallOnceType``
-  ``PRCallOnceFN``
-  ``PR_CallOnce``
