.. _mozilla_projects_nss_reference_fc_decryptverifyupdate:

FC_DecryptVerifyUpdate
======================

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_DecryptVerifyUpdate - continue a multi-part decrypt and verify operation

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code:: eval

      CK_RV FC_DecryptVerifyUpdate(
        CK_SESSION_HANDLE hSession,
        CK_BYTE_PTR pEncryptedData,
        CK_ULONG ulEncryptedDataLen,
        CK_BYTE_PTR pData,
        CK_ULONG_PTR pulDataLen
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``hSession``
      [in] session handle.
   ``pEncryptedData``
      [in] pointer to the encrypted data part.
   ``ulEncryptedDataLen``
      [in] length of encrypted data in bytes.
   ``pData``
      [in] pointer to the location which receives the recovered data part or NULL.
   ``pulDataLen``
      [in] pointer to the length of the recovered part buffer.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_DecryptVerifyUpdate`` continues a multi-part decryption and signature verification
   operation. After calling both ``FC_DecryptInit`` and ``FC_VerifyInit`` to set up the operations
   this function may be called multiple times. The operation is finished by calls to
   ``FC_DecryptFinal`` and ``FC_VerifyFinal``.

   A user must log into the token (to assume the NSS User role) before calling
   ``FC_DecryptVerifyUpdate``.

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

   -  `NSC_DecryptVerifyUpdate </en-US/NSC_DecryptVerifyUpdate>`__