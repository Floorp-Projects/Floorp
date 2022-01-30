.. _mozilla_projects_nss_tools_nss_tools_dbck-tasks:

NSS Tools dbck-tasks
====================

.. _nss_security_tools_dbck_tasks:

`NSS Security Tools: dbck Tasks <#nss_security_tools_dbck_tasks>`__
-------------------------------------------------------------------

.. container::

   Newsgroup: `mozilla.dev.tech.crypto <news://news.mozilla.org/mozilla.dev.tech.crypto>`__

.. _task_list:

`Task List <#task_list>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   #. In analyze mode, there should be an option to create a file containing a graph of the
      certificate database without any information about the user's certificates (no common names,
      email addresses, etc.). This file could be mailed to a mail alias to assist in finding the
      source of database corruption.
   #. The dbck tool should be able to repair a currupted database. There should be command-line
      options and, perhaps, an interactive mode to allow determine which certificates to keep.
   #. The dbck tool should be able to update a databa