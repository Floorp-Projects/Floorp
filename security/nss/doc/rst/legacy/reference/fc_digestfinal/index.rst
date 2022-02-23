.. _mozilla_projects_nss_reference_fc_digestfinal:

FC_DigestFinal
==============

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_DigestFinal - finish a multi-part digest operation.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code:: eval

      CK_RV FC_DigestFinal(
        CK_SESSION_HANDLE hSession,
        CK_BYTE_PTR pDigest,
        CK_ULONG_PTR pulDigestLen
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``hSession``
      [in] session handle.
   ``pDigest``
      [out] pointer to the buffer which will receive the digest or NULL.
   ``pulDigestLen``
      [in, out] pointer to location containing the maximum buffer size.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_DigestFinal`` finishes a multi-part digest operation by returning the complete digest and
   clearing the operation context. If ``pDigest`` is NULL the length of the digest is returned and
   ``FC_DigestFinal`` may be called again with ``pDigest`` set to retrieve the digest.

   A user may call ``FC_DigestFinal`` without logging into the token (to assume the NSS User role).

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

   -  :ref:`mozilla_projects_nss_reference_fc_digestinit`,
      `NSC_DigestFinal </en-US/NSC_DigestFinal>`__