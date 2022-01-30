.. _mozilla_projects_nss_reference_fc_seedrandom:

FC_SeedRandom
=============

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   ``FC_SeedRandom()`` - mix additional seed material into the random number generator.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code:: eval

      CK_RV FC_SeedRandom(
        CK_SESSION_HANDLE hSession,
        CK_BYTE_PTR pSeed,
        CK_ULONG usSeedLen
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``hSession``
      [in] session handle.
   ``pSeed``
      [in] pointer to the seed material
   ``usSeedLen``
      [in] length of the seed material in bytes.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_SeedRandom()`` mixes additional seed material into the token's random number generator. Note
   that ``FC_SeedRandom()`` doesn't provide the initial seed material for the random number
   generator. The initial seed material is provided by the NSS cryptographic module itself.

   | 
   | A user may call ``FC_SeedRandom()`` without logging into the token (to assume the NSS User
     role).

.. _return_value:

`Return value <#return_value>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

`Examples <#examples>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

.. _see_also:

`See also <#see_also>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  `NSC_SeedRandom </en-US/NSC_SeedRandom>`__