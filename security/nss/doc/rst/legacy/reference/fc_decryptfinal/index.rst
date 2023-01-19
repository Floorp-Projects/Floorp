.. _mozilla_projects_nss_reference_fc_decryptfinal:

FC_DecryptFinal
===============

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_DecryptFinal - finish a multi-part decryption operation.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code::

      CK_RV FC_DecryptFinal(
        CK_SESSION_HANDLE hSession,
        CK_BYTE_PTR pLastPart,
        CK_ULONG_PTR pusLastPartLen
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``hSession``
      [in] session handle.
   ``pLastPart``
      [out] pointer to the location where the last block of recovered data, if any, is to be stored.
   ``pusLastPartLen``
      [in,out] pointer to location where the number of bytes of recovered data is to be stored.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_DecryptFinal`` returns the last block of data of a multi-part decryption operation.

   A user must log into the token (to assume the NSS User role) before calling ``FC_DecryptFinal``.

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

   -  :ref:`mozilla_projects_nss_reference_fc_decryptinit`,
      `NSC_DecryptFinal </en-US/NSC_DecryptFinal>`__