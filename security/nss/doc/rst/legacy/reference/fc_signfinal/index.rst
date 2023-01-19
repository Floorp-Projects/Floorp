.. _mozilla_projects_nss_reference_fc_signfinal:

FC_SignFinal
============

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_SignFinal - finish a multi-part signing operation.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code::

      CK_RV FC_SignFinal(
        CK_SESSION_HANDLE hSession,
        CK_BYTE_PTR pSignature,
        CK_ULONG_PTR pusSignatureLen
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``hSession``
      [in] session handle.
   ``pSignature``
      [out] pointer to the buffer which will receive the digest or NULL.
   ``pusSignatureLen``
      [in, out] pointer to location containing the maximum buffer size.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_SignFinal`` finishes a multi-part signing operation by returning the complete signature and
   clearing the operation context. If ``pSignature`` is NULL the length of the signature is returned
   and ``FC_SignFinal`` may be called again with ``pSignature`` set to retrieve the signature.

   A user must log into the token (to assume the NSS User role) before calling ``FC_SignFinal``.

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

   -  :ref:`mozilla_projects_nss_reference_fc_signupdate`, `NSC_SignFinal </en-US/NSC_SignFinal>`__