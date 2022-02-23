.. _mozilla_projects_nss_reference_fc_closeallsessions:

FC_CloseAllSessions
===================

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_CloseAllSessions - close all sessions between an application and a token.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code:: eval

      CK_RV FC_CloseAllSessions(
        CK_SLOT_ID slotID
       );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``slotID``
      [in] the ID of the token's slot.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_CloseAllSessions`` closes all sessions between an application and the token in the slot with
   the ID ``slotID``.

   The NSS cryptographic module currently doesn't call the surrender callback function ``Notify``.
   (See PKCS #11 v2.20 section 11.17.1.)

   A user may call ``FC_CloseAllSessions`` without logging into the token (to assume the NSS User
   role).

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
      `NSC_CloseAllSessions </en-US/NSC_CloseAllSessions>`__