.. _mozilla_projects_nss_reference_fc_verifyupdate:

FC_VerifyUpdate
===============

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_VerifyUpdate - process the next block of a multi-part verify operation.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code:: eval

      CK_RV FC_VerifyUpdate(
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
      [in] pointer to the next block of the data to be verified.
   ``usPartLen``
      [in] length of data block in bytes.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_VerifyUpdate`` starts or continues a multi-part signature verification operation where the
   signature is an appendix to the data. One or more blocks may be part of the signature. The result
   for the entire message is returned by a call to
   :ref:`mozilla_projects_nss_reference_fc_verifyfinal`.

   A user must log into the token (to assume the NSS User role) before calling ``FC_VerifyUpdate``.

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

   -  :ref:`mozilla_projects_nss_reference_fc_verifyfinal`,
      `NSC_VerifyUpdate </en-US/NSC_VerifyUpdate>`__