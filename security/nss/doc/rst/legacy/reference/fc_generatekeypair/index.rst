.. _mozilla_projects_nss_reference_fc_generatekeypair:

FC_GenerateKeyPair
==================

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_GenerateKeyPair - generate a new public/private key pair

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code::

      CK_RV FC_GenerateKeyPair(
        CK_SESSION_HANDLE hSession,
        CK_MECHANISM_PTR pMechanism,
        CK_ATTRIBUTE_PTR pPublicKeyTemplate,
        CK_ULONG usPublicKeyAttributeCount,
        CK_ATTRIBUTE_PTR pPrivateKeyTemplate,
        CK_ULONG usPrivateKeyAttributeCount,
        CK_OBJECT_HANDLE_PTR phPublicKey,
        CK_OBJECT_HANDLE_PTR phPrivateKey
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``hSession``
      [in] session handle.
   ``pMechanism``
      [in] pointer to the mechanism to use.
   ``pPublicKeyTemplate``
      [in] pointer to the public key template.
   ``usPublicKeyAttributeCount``
      [in] number of attributes in the public key template.
   ``pPrivateKeyTemplate``
      [in] pointer to the private key template.
   ``usPrivateKeyAttributeCount``
      [in] number of attributes in the private key template.
   ``phPublicKey``
      [out] pointer to the location to receive the handle of the new public key.
   ``phPrivateKey``
      [out] pointer to the location to receive the handle of the new private key.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_GenerateKeyPair`` generates a public/private key pair, creating new key objects. The handles
   of new keys are returned.

   A user must log into the token (to assume the NSS User role) before calling
   ``FC_GenerateKeyPair``.

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

   -  `NSC_GenerateKeyPair </en-US/NSC_GenerateKeyPair>`__