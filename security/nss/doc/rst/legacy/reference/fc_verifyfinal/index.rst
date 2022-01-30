.. _mozilla_projects_nss_reference_fc_verifyfinal:

FC_VerifyFinal
==============

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_VerifyFinal - finish a multi-part verify operation.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code:: eval

      CK_RV FC_VerifyFinal(
        CK_SESSION_HANDLE hSession,
        CK_BYTE_PTR pSignature,
        CK_ULONG usSignatureLen
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``hSession``
      [in] session handle.
   ``pSignature``
      [in] pointer to the buffer which will receive the digest or NULL.
   ``usSignatureLen``
      [in] length of the signature in bytes.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_VerifyFinal`` finishes a multi-part signature verification operation.

   A user must log into the token (to assume the NSS User role) before calling ``FC_VerifyFinal``.

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

   -  :ref:`mozilla_projects_nss_reference_fc_verifyupdate`,
      `NSC_VerifyFinal </en-US/NSC_VerifyFinal>`__