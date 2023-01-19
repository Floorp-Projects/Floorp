.. _mozilla_projects_nss_reference_fc_sign:

FC_Sign
=======

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_Sign - sign a block of data.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code::

      CK_RV FC_Sign(
        CK_SESSION_HANDLE hSession,
        CK_BYTE_PTR pData,
        CK_ULONG usDataLen,
        CK_BYTE_PTR pSignature,
        CK_ULONG_PTR pusSignatureLen
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
      [out] pointer to location where recovered data is to be stored.
   ``pusSignatureLen``
      [in, out] pointer to the maximum size of the output buffer, replaced by the length of the
      signature if the operation is successful.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_Sign`` signs a message in a single operation according to the attributes of the previous
   call to ``FC_SignInit``.

   A user must log into the token (to assume the NSS User role) before calling ``FC_Sign``.

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

   -  :ref:`mozilla_projects_nss_reference_fc_signinit`, `NSC_Sign </en-US/NSC_Sign>`__