.. _mozilla_projects_nss_reference_fc_decryptinit:

FC_DecryptInit
==============

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_DecryptInit - initialize a decryption operation.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code::

      CK_RV FC_DecryptInit(
        CK_SESSION_HANDLE hSession,
        CK_MECHANISM_PTR pMechanism,
        CK_OBJECT_HANDLE hKey
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``hSession``
      [in] session handle.
   ``pMechanism``
      [in] mechanism to be used for the subsequent decryption operation.
   ``hKey``
      [in] handle of the key to be used.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_DecryptInit`` initializes a decryption operation.

   A user must log into the token (to assume the NSS User role) before calling ``FC_DecryptInit``.

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

   -  `NSC_DecryptInit </en-US/NSC_DecryptInit>`__