.. _mozilla_projects_nss_reference_fc_opensession:

FC_OpenSession
==============

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_OpenSession - open a session between an application and a token.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code::

      CK_RV FC_OpenSession(
        CK_SLOT_ID slotID,
        CK_FLAGS flags,
        CK_VOID_PTR pApplication,
        CK_NOTIFY Notify,
        CK_SESSION_HANDLE_PTR phSession
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_OpenSession`` has the following parameters:

   ``slotID``
      [in] the ID of the token's slot.
   ``flags``
      [in]
   ``pApplication``
   ``Notify``
      [in] pointer to a notification callback function. Not currently supported.
   ``phSession``
      [out] pointer to a session handle.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_OpenSession`` opens a session between an application and the token in the slot with the ID
   ``slotID``.

   The NSS cryptographic module currently doesn't call the surrender callback function ``Notify``.
   (See PKCS #11 v2.20 section 11.17.1.)

   A user may call ``FC_OpenSession`` without logging into the token (to assume the NSS User role).

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

   -  :ref:`mozilla_projects_nss_reference_fc_closesession`,
      `NSC_OpenSession </en-US/NSC_OpenSession>`__