.. _mozilla_projects_nss_reference_fc_encryptinit:

FC_EncryptInit
==============

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_EncryptInit - initialize an encryption operation.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code:: eval

      CK_RV FC_EncryptInit(
        CK_SESSION_HANDLE hSession,
        CK_MECHANISM_PTR pMechanism,
        CK_OBJECT_HANDLE hKey
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``hSession``
      [in] handle to the session.
   ``pMechanism``
      [in] pointer to the mechanism to be used for subsequent encryption.
   ``hKey``
      [in] handle of the encryption key.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_EncryptInit`` initializes an encryption operation with the mechanism and key to be used.

   A user must log into the token (to assume the NSS User role) before calling ``FC_EncryptInit``.

.. _return_value:

`Return value <#return_value>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``CKR_OK``
      Slot information was successfully copied.
   ``CKR_SLOT_ID_INVALID``
      The specified slot number is out of the defined range of values.

`Examples <#examples>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

.. _see_also:

`See also <#see_also>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  `NSC_EncryptInit </en-US/NSC_EncryptInit>`__