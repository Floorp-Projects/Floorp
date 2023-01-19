.. _mozilla_projects_nss_reference_fc_signencryptupdate:

FC_SignEncryptUpdate
====================

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_SignEncryptUpdate - continue a multi-part signing and encryption operation

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code::

      CK_RV FC_SignEncryptUpdate(
        CK_SESSION_HANDLE hSession,
        CK_BYTE_PTR pPart,
        CK_ULONG ulPartLen,
        CK_BYTE_PTR pEncryptedPart,
        CK_ULONG_PTR pulEncryptedPartLen
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``hSession``
      [in] session handle.
   ``pPart``
      [in] pointer to the data part.
   ``ulPartLen``
      [in] length of data in bytes.
   ``pEncryptedPart``
      [in] pointer to the location which receives the signed and encrypted data part or NULL.
   ``pulEncryptedPartLen``
      [in] pointer to the length of the encrypted part buffer.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_SignEncryptUpdate`` continues a multi-part signature and encryption operation. After calling
   both ``FC_SignInit`` and ``FC_EncryptInit`` to set up the operations this function may be called
   multiple times. The operation is finished by calls to ``FC_SignFinal`` and ``FC_EncryptFinal``.

   A user must log into the token (to assume the NSS User role) before calling
   ``FC_SignEncryptUpdate``.

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

   -  `NSC_SignEncryptUpdate </en-US/NSC_SignEncryptUpdate>`__