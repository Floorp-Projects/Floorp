.. _mozilla_projects_nss_reference_fc_findobjects:

FC_FindObjects
==============

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_FindObjects - Search for one or more objects

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code:: eval

      CK_RV FC_FindObjects(
        CK_SESSION_HANDLE hSession,
        CK_OBJECT_HANDLE_PTR phObject,
        CK_ULONG usMaxObjectCount,
        CK_ULONG_PTR pusObjectCount
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``hSession``
      [in] session handle.
   ``pTemplate``
      [out] pointer to location to receive the object handles.
   ``usMaxObjectCount``
      [in] maximum number of handles to retrieve.
   ``pusObjectCount``
      [out] pointer to location to receive the number of returned handles.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_FindObjects`` returns the next set of object handles matching the criteria set up by the
   previous call to ``FC_FindObjectsInit`` and sets the object count variable to their number or to
   zero if there are none.

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

   -  :ref:`mozilla_projects_nss_reference_fc_findobjectsinit`,
      `NSC_FindObjects </en-US/NSC_FindObjects>`__