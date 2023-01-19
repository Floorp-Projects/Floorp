.. _mozilla_projects_nss_reference_fc_digestkey:

FC_DigestKey
============

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_DigestKey - add the digest of a key to a multi-part digest operation.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code::

      CK_RV FC_DigestKey(
        CK_SESSION_HANDLE hSession,
        CK_OBJECT_HANDLE hKey
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``hSession``
      [in] session handle.
   ``hKey``
      [in] handle of the key to be digested.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_DigestKey`` continues a multi-part digest operation by digesting the value of a secret key.
   The digest for the entire message is returned by a call to
   :ref:`mozilla_projects_nss_reference_fc_digestfinal`.

   A user must log into the token (to assume the NSS User role) before calling ``FC_DigestKey``.

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
      :ref:`mozilla_projects_nss_reference_fc_digestfinal`, `NSC_DigestKey </en-US/NSC_DigestKey>`__