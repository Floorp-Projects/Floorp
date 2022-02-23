.. _mozilla_projects_nss_reference_fc_closesession:

FC_CloseSession
===============

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_CloseSession - close a session opened between an application and a token.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code:: eval

      CK_RV FC_CloseSession(
        CK_SESSION_HANDLE hSession
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``hSession``
      [in] the session handle to be closed.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_CloseSession`` closes a session between an application and a token.

   A user may call ``FC_CloseSession`` without logging into the token (to assume the NSS User role).

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

   -  :ref:`mozilla_projects_nss_reference_fc_opensession`