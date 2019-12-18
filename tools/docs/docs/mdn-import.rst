Importing documentation from MDN
--------------------------------

As MDN should not be used for documenting mozilla-central specific code or process,
the documentation should be migrated in this repository.

Fortunatelly, there is an easy way to import the doc from MDN
to the firefox source docs.

1. Install https://pandoc.org/

2. Add a ``?raw=1`` add the end of the MDN URL

3. Run pandoc the following way:

.. code-block:: shell

   $ pandoc -t rst https://wiki.developer.mozilla.org/docs/Web/JavaScript?raw\=1  > doc.rst

4. Verify the rst syntax using `./mach lint -l rst`_

.. _./mach lint -l rst: /tools/lint/linters/rstlinter.html
