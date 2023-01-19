.. _mozilla_projects_nss_reference_fc_getsessioninfo:

FC_GetSessionInfo
=================

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_GetSessionInfo - obtain information about a session.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code::

      CK_RV FC_GetSessionInfo(
        CK_SESSION_HANDLE hSession,
        CK_SESSION_INFO_PTR pInfo
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``hSession``
      [in] the open session handle.
   ``pInfo``
      [out] pointer to the `CK_SESSION_INFO </en-US/CK_SESSION_INFO>`__ structure to be returned.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_GetSessionInfo`` obtains information about a session. A user may call ``FC_GetSessionInfo``
   without logging into the token (to assume the NSS User role).

   If the NSS cryptographic module is in the error state, ``FC_GetSessionInfo`` returns
   ``CKR_DEVICE_ERROR``. Otherwise, it fills in the ``CK_SESSION_INFO`` structure with the following
   information:

   -  ``state``: the state of the session, i.e., no role is assumed, the User role is assumed, or
      the Crypto Officer role is assumed
   -  ``flags``: bit flags that define the type of session

      -  ``CKF_RW_SESSION (0x00000002)``: true if the session is read/write; false if the session is
         read-only.
      -  ``CKF_SERIAL_SESSION (0x00000004)``: this flag is provided for backward compatibility and
         is always set to true.

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