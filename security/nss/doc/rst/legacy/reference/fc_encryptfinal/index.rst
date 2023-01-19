.. _mozilla_projects_nss_reference_fc_encryptfinal:

FC_EncryptFinal
===============

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_EncryptFinal - finish a multi-part encryption operation.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code::

      CK_RV FC_EncryptFinal(
        CK_SESSION_HANDLE hSession,
        CK_BYTE_PTR pLastEncryptedPart,
        CK_ULONG_PTR pusLastEncryptedPartLen
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``hSession``
      [in] session handle.
   ``pLastEncryptedPart``
      [out] pointer to the location that receives the last encrypted data part, if any
   ``pusLastEncryptedPartLen``
      [in,out] pointer to location where the number of bytes of the last encrypted data part is to
      be stored.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_EncryptFinal`` returns the last block of data of a multi-part encryption operation.

   A user must log into the token (to assume the NSS User role) before calling ``FC_EncryptFinal``.

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

   -  :ref:`mozilla_projects_nss_reference_fc_encryptinit`,
      `NSC_EncryptFinal </en-US/NSC_EncryptFinal>`__