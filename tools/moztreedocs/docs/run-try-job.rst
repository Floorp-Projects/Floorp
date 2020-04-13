Running a try job for Documentation
-----------------------------------

Documentation has two try jobs associated :

  - ``doc-generate`` - This generates the documentation with the committed changes on the try server and gives the same output as if it has landed on regular integration branch.

   .. code-block:: shell

      mach try fuzzy -q "'doc-generate"

  - ``doc-upload`` - This uploads documentation to `gecko-l1 bucket <http://gecko-docs.mozilla.org-l1.s3.us-west-2.amazonaws.com/index.html>`__ with the committed changes.
  
   .. code-block:: shell

      mach try fuzzy -q "'doc-upload"

.. important::
   
   Running try jobs require the user to have try server access.

.. note::

   To learn more about setting up try server or
   using a different selector head over to :ref:`try server documentation <Try Server>`
