.. _mozilla_projects_nss_reference_fc_signupdate:

FC_SignUpdate
=============

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_SignUpdate - process the next block of a multi-part signing operation.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code::

      CK_RV FC_SignUpdate(
        CK_SESSION_HANDLE hSession,
        CK_BYTE_PTR pPart,
        CK_ULONG usPartLen
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``hSession``
      [in] session handle.
   ``pPart``
      [in] pointer to the next block of the data to be signed.
   ``usPartLen``
      [in] length of data block in bytes.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_SignUpdate`` starts or continues a multi-part signature operation. One or more blocks may be
   part of the signature. The signature for the entire message is returned by a call to
   :ref:`mozilla_projects_nss_reference_fc_signfinal`.

   A user must log into the token (to assume the NSS User role) before calling ``FC_SignUpdate``.

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

   -  :ref:`mozilla_projects_nss_reference_fc_signinit`,
      :ref:`mozilla_projects_nss_reference_fc_signfinal`, `NSC_SignUpdate </en-US/NSC_SignUpdate>`__