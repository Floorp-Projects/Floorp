.. _mozilla_projects_nss_reference_fc_signrecoverinit:

FC_SignRecoverInit
==================

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_SignRecoverInit - initialize a sign recover operation.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code:: eval

      CK_RV FC_SignRecoverInit(
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
      [in] mechanism to be used for the signing operation.
   ``hKey``
      [in] handle of the key to be used.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_SignRecoverInit`` initializes a initializes a signature operation where the (digest) data
   can be recovered from the signature.

   A user must log into the token (to assume the NSS User role) before calling
   ``FC_SignRecoverInit``.

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

   -  `NSC_SignRecoverInit </en-US/NSC_SignRecoverInit>`__