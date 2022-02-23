.. _mozilla_projects_nss_reference_fc_generaterandom:

FC_GenerateRandom
=================

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_GenerateRandom - generate a random number.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code:: eval

      CK_RV FC_GenerateRandom(
        CK_SESSION_HANDLE hSession,
        CK_BYTE_PTR pRandomData,
        CK_ULONG ulRandomLen
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``hSession``
      [in] session handle.
   ``pRandomData``
      [out] pointer to the location to receive the random data.
   ``ulRandomLen``
      [in] length of the buffer in bytes.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_GenerateRandom`` generates random data of the specified length.

   A user may call ``FC_GenerateRandom`` without logging into the token (to assume the NSS User
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

   -  `NSC_GenerateRandom </en-US/NSC_GenerateRandom>`__