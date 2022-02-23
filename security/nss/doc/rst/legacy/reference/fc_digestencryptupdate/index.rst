.. _mozilla_projects_nss_reference_fc_digestencryptupdate:

FC_DigestEncryptUpdate
======================

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_DigestEncryptUpdate - continue a multi-part digest and encryption operation

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code:: eval

      CK_RV FC_DigestEncryptUpdate(
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
      [in] pointer to the location which receives the digested and encrypted part or NULL.
   ``pulEncryptedPartLen``
      [in] pointer to the length of the encrypted part buffer.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_DigestEncryptUpdate`` continues a multi-part digest and encryption operation. After calling
   both ``FC_DigestInit`` and ``FC_EncryptInit`` to set up the operations this function may be
   called multiple times. The operation is finished by calls to ``FC_DigestFinal`` and
   ``FC_EncryptFinal`` in that order.

   A user must log into the token (to assume the NSS User role) before calling
   ``FC_DigestEncryptUpdate``.

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

   -  `NSC_DigestEncryptUpdate </en-US/NSC_DigestEncryptUpdate>`__