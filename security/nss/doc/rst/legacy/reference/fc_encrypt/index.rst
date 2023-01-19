.. _mozilla_projects_nss_reference_fc_encrypt:

FC_Encrypt
==========

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_Encrypt - Encrypt a block of data.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code::

      CK_RV FC_Encrypt(
        CK_SESSION_HANDLE hSession,
        CK_BYTE_PTR pData,
        CK_ULONG usDataLen,
        CK_BYTE_PTR pEncryptedData,
        CK_ULONG_PTR pusEncryptedDataLen
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``hSession``
      [in] session handle.
   ``pData``
      [in] pointer to the data buffer
   ``usDataLen``
      [in] length of the data buffer in bytes.
   ``pEncryptedData``
      [out] pointer to location where encrypted data is to be stored.
   ``pusEncryptedDataLen``
      [in/out] number of bytes.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_Encrypt`` encrypts a block of data according to the attributes of the previous call to
   ``FC_EncryptInit``.

   A user must log into the token (to assume the NSS User role) before calling ``FC_Encrypt``.

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

   -  :ref:`mozilla_projects_nss_reference_fc_encryptinit`, `NSC_Encrypt </en-US/NSC_Encrypt>`__