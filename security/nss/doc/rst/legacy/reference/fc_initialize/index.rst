.. _mozilla_projects_nss_reference_fc_initialize:

FC_Initialize
=============

.. _name:

`Summary <#name>`__
-------------------

.. container::

   FC_Initialize - initialize the PKCS #11 library.

`Syntax <#syntax>`__
--------------------

.. container::

   .. code::

      CK_RV FC_Initialize(CK_VOID_PTR pInitArgs);

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``pInitArgs``
      Points to a ``CK_C_INITIALIZE_ARGS`` structure.

`Description <#description>`__
------------------------------

.. container::

   ``FC_Initialize`` initializes the :ref:`mozilla_projects_nss_reference_nss_cryptographic_module`
   for the :ref:`mozilla_projects_nss_reference_nss_cryptographic_module_fips_mode_of_operation`. In
   addition to creating the internal data structures, it performs the FIPS software integrity test
   and power-up self-tests.

   The ``pInitArgs`` argument must point to a ``CK_C_INITIALIZE_ARGS`` structure whose members
   should have the following values:

   -  ``CreateMutex`` should be ``NULL``.
   -  ``DestroyMutex`` should be ``NULL``.
   -  ``LockMutex`` should be ``NULL``.
   -  ``UnlockMutex`` should be ``NULL``.
   -  ``flags`` should be ``CKF_OS_LOCKING_OK``.
   -  ``LibraryParameters`` should point to a string that contains the library parameters.
   -  ``pReserved`` should be ``NULL``.

   The library parameters string has this format:

   .. code::

      "configdir='dir' certPrefix='prefix1' keyPrefix='prefix2' secmod='file' flags= "

   Here are some examples.

   ``NSS_NoDB_Init("")``, which initializes NSS with no databases:

   .. code::

       "configdir='' certPrefix='' keyPrefix='' secmod='' flags=readOnly,noCertDB,noMod
      DB,forceOpen,optimizeSpace "

   Mozilla Firefox initializes NSS with this string (on Windows):

   .. code::

       "configdir='C:\\Documents and Settings\\wtc\\Application Data\\Mozilla\\Firefox\\Profiles\\default.7tt' certPrefix='' keyPrefix='' secmod='secmod.db' flags=optimizeSpace  manufacturerID='Mozilla.org' libraryDescription='PSM Internal Crypto Services' cryptoTokenDescription='Generic Crypto Services' dbTokenDescription='Software Security Device' cryptoSlotDescription='PSM Internal Cryptographic Services' dbSlotDescription='PSM Private Keys' FIPSSlotDescription='PSM Internal FIPS-140-1 Cryptographic Services' FIPSTokenDescription='PSM FIPS-140-1 User Private Key Services' minPS=0"

   See :ref:`mozilla_projects_nss_pkcs11_module_specs` for complete documentation of the library
   parameters string.

.. _return_value:

`Return value <#return_value>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_Initialize`` returns the following return codes.

   -  ``CKR_OK``: library initialization succeeded.
   -  ``CKR_ARGUMENTS_BAD``

      -  ``pInitArgs`` is ``NULL``.
      -  ``pInitArgs->LibraryParameters`` is ``NULL``.
      -  only some of the lock functions were provided by the application.

   -  ``CKR_CANT_LOCK``: the ``CKF_OS_LOCKING_OK`` flag is not set in ``pInitArgs->flags``. The NSS
      cryptographic module always uses OS locking and doesn't know how to use the lock functions
      provided by the application.
   -  ``CKR_CRYPTOKI_ALREADY_INITIALIZED``: the library is already initialized.
   -  ``CKR_DEVICE_ERROR``

      -  We failed to create the OID tables, random number generator, or internal locks. (Note: we
         probably should return ``CKR_HOST_MEMORY`` instead.)
      -  The software integrity test or power-up self-tests failed. The NSS cryptographic module is
         in a fatal error state.

   -  ``CKR_HOST_MEMORY``: we ran out of memory.

`Examples <#examples>`__
------------------------

.. container::

   .. code::

      #include <assert.h>

      CK_FUNCTION_LIST_PTR pFunctionList;
      CK_RV crv;
      CK_C_INITIALIZE_ARGS initArgs;

      crv = FC_GetFunctionList(&pFunctionList);
      assert(crv == CKR_OK);

      initArgs.CreateMutex = NULL;
      initArgs.DestroyMutex = NULL;
      initArgs.LockMutex = NULL;
      initArgs.UnlockMutex = NULL;
      initArgs.flags = CKF_OS_LOCKING_OK;
      initArgs.LibraryParameters = "...";
      initArgs.pReserved = NULL;

      /* invoke FC_Initialize as pFunctionList->C_Initialize */
      crv = pFunctionList->C_Initialize(&initArgs);
