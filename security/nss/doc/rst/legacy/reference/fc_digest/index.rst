.. _mozilla_projects_nss_reference_fc_digest:

FC_Digest
=========

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_Digest - digest a block of data.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code::

      CK_RV FC_Digest(
        CK_SESSION_HANDLE hSession,
        CK_BYTE_PTR pData,
        CK_ULONG usDataLen,
        CK_BYTE_PTR pDigest,
        CK_ULONG_PTR pusDigestLen
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
   ``pDigest``
      [out] pointer to location where recovered data is to be stored.
   ``pusDigestLen``
      [in, out] pointer to the maximum size of the output buffer, replaced by the length of the
      message digest if the operation is successful.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_Digest`` digests a message in a single operation according to the attributes of the previous
   call to ``FC_DigestInit``.

   A user may call ``FC_Digest`` without logging into the token (to assume the NSS User role).

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

   -  :ref:`mozilla_projects_nss_reference_fc_digestinit`, `NSC_Digest </en-US/NSC_Digest>`__