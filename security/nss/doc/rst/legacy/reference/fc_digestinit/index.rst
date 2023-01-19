.. _mozilla_projects_nss_reference_fc_digestinit:

FC_DigestInit
=============

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_DigestInit - initialize a message-digest operation.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code::

      CK_RV FC_DigestInit(
        CK_SESSION_HANDLE hSession,
        CK_MECHANISM_PTR pMechanism
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``hSession``
      [in] session handle.
   ``pMechanism``
      [in] mechanism to be used for the subsequent digest operation.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_DigestInit`` initializes a message-digest operation.

   A user may call ``FC_DigestInit`` without logging into the token (to assume the NSS User role).

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

   -  `NSC_DigestInit </en-US/NSC_DigestInit>`__