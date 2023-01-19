.. _mozilla_projects_nss_reference_fc_findobjectsfinal:

FC_FindObjectsFinal
===================

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_FindObjectsFinal - terminate an object search.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code::

      CK_RV FC_FindObjectsFinal(
        CK_SESSION_HANDLE hSession,
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``hSession``
      [in] session handle.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Clears the object search criteria for a session.

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

   -  :ref:`mozilla_projects_nss_reference_fc_findobjects`,
      `NSC_FindObjectsFinal </en-US/NSC_FindObjectsFinal>`__