.. _mozilla_projects_nss_reference_fc_getmechanisminfo:

FC_GetMechanismInfo
===================

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_GetMechanismInfo - get information on a particular mechanism.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code::

      CK_RV FC_GetMechanismInfo(
        CK_SLOT_ID slotID,
        CK_MECHANISM_TYPE type,
        CK_MECHANISM_INFO_PTR pInfo
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_GetMechanismInfo`` takes three parameters:

   ``slotID``
      [Input]
   ``type``
      [Input] .
   ``pInfo``
      [Output] .

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_GetMechanismInfo`` obtains information about a particular mechanism possibly supported by a
   token.

   A user may call ``FC_GetMechanismInfo`` without logging into the token (to assume the NSS User
   role).

.. _return_value:

`Return value <#return_value>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``CKR_OK``

`Examples <#examples>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

.. _see_also:

`See also <#see_also>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  `NSC_GetMechanismInfo </en-US/NSC_GetMechanismInfo>`__