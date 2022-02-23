.. _mozilla_projects_nss_reference_fc_digestupdate:

FC_DigestUpdate
===============

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_DigestUpdate - process the next block of a multi-part digest operation.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code:: eval

      CK_RV FC_DigestUpdate(
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
      [in] pointer to the next block of data to be digested.
   ``usPartLen``
      [in] length of data block in bytes.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_DigestUpdate`` starts or continues a multi-part digest operation. One or more blocks may be
   part of the message digest operation. The digest for the entire message is returned by a call to
   :ref:`mozilla_projects_nss_reference_fc_digestfinal`.

   A user may call ``FC_DigestUpdate`` without logging into the token (to assume the NSS User role).

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
      :ref:`mozilla_projects_nss_reference_fc_digestfinal`,
      `NSC_DigestUpdate </en-US/NSC_DigestUpdate>`__