.. _mozilla_projects_nss_reference_fc_decryptupdate:

FC_DecryptUpdate
================

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_DecryptUpdate - decrypt a block of a multi-part encryption operation.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code::

      CK_RV FC_DecryptUpdate(
        CK_SESSION_HANDLE hSession,
        CK_BYTE_PTR pEncryptedPart,
        CK_ULONG usEncryptedPartLen,
        CK_BYTE_PTR pPart,
        CK_ULONG_PTR pusPartLen
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``hSession``
      [in] session handle.
   ``pEncryptedPart``
      [in] pointer to the next block of data to be decrypted.
   ``usEncryptedPartLen``
      [in] length of data block in bytes.
   ``pPart``
      [out] pointer to location where recovered block is to be stored.
   ``pusPartLen``
      [in,out] pointer the location where the number of bytes of recovered data is to be stored.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_DecryptUpdate`` decrypts a block of data according to the attributes of the previous call to
   ``FC_DecryptInit``. The block may be part of a multi-part decryption operation.

   A user must log into the token (to assume the NSS User role) before calling ``FC_DecryptUpdate``.

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
      `NSC_DecryptUpdate </en-US/NSC_DecryptUpdate>`__