.. _mozilla_projects_nss_reference_fc_encryptupdate:

FC_EncryptUpdate
================

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_EncryptUpdate - encrypt a block of a multi-part encryption operation.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code:: eval

      CK_RV FC_EncryptUpdate(
        CK_SESSION_HANDLE hSession,
        CK_BYTE_PTR pPart,
        CK_ULONG usPartLen,
        CK_BYTE_PTR pEncryptedPart,
        CK_ULONG_PTR pusEncryptedPartLen
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``hSession``
      [in] session handle.
   ``pPart``
      [in] pointer to the next block of data to be encrypted.
   ``usPartLen``
      [in] length of data block in bytes.
   ``pEncryptedPart``
      [out] pointer to location where encrypted block is to be stored.
   ``pusEncryptedPartaLen``
      [out] pointer the location where the number of bytes of encrypted data is to be stored.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_EncryptUpdate`` encrypts a block of data according to the attributes of the previous call to
   ``FC_EncryptInit``. The block may be part of a multi-part encryption operation.

   A user must log into the token (to assume the NSS User role) before calling ``FC_EncryptUpdate``.

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
      `NSC_EncryptUpdate </en-US/NSC_EncryptUpdate>`__