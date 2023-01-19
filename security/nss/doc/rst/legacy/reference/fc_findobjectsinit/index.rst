.. _mozilla_projects_nss_reference_fc_findobjectsinit:

FC_FindObjectsInit
==================

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_FindObjectsInit - initialize the parameters for an object search.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code::

      CK_RV FC_FindObjectsInit(
        CK_SESSION_HANDLE hSession,
        CK_ATTRIBUTE_PTR pTemplate,
        CK_ULONG usCount
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``hSession``
      [in] session handle.
   ``pTemplate``
      [in] pointer to template.
   ``usCount``
      [in] number of attributes in the template.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_FindObjectsInit`` sets the attribute list for an object search. If ``FC_FindObjectsInit`` is
   successful ``FC_FindObjects`` may be called one or more times to retrieve handles of matching
   objects.

   A user must log into the token (to assume the NSS User role) before searching for secret or
   private key objects.

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
      `NSC_FindObjectsInit </en-US/NSC_FindObjectsInit>`__