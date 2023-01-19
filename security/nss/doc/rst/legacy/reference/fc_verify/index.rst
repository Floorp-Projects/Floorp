.. _mozilla_projects_nss_reference_fc_verify:

FC_Verify
=========

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_Verify - sign a block of data.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code::

      CK_RV FC_Verify(
        CK_SESSION_HANDLE hSession,
        CK_BYTE_PTR pData,
        CK_ULONG usDataLen,
        CK_BYTE_PTR pSignature,
        CK_ULONG usSignatureLen
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``hSession``
      [in] session handle.
   ``pData``
      [in] pointer to data block.
   ``usDataLen``
      [in] length of the data in bytes.
   ``pSignature``
      [in] pointer to the signature.
   ``usSignatureLen``
      [in] length of the signature in bytes.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_Verify`` verifies a signature in a single-part operation, where the signature is an appendix
   to the data.

   A user must log into the token (to assume the NSS User role) before calling ``FC_Verify``.

.. _return_value:

`Return value <#return_value>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``CKR_OK`` is returned on success. ``CKR_SIGNATURE_INVALID`` is returned for signature mismatch.

`Examples <#examples>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

.. _see_also:

`See also <#see_also>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  :ref:`mozilla_projects_nss_reference_fc_verifyinit`, `NSC_Verify </en-US/NSC_Verify>`__