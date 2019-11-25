Importing documentation from MDN
--------------------------------

As MDN should not be used for documenting mozilla-central specific code or process,
the documentation should be migrated in this repository.

Fortunatelly, there is an easy way to import the doc from MDN
to the firefox source docs.

1. Install https://pandoc.org/

2. Add a ``?raw=1`` add the end of the MDN URL

3. Run pandoc the following way:
   ``pandoc -t rst https://wiki.developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Build_Instructions/Simple_Firefox_build/Linux_and_MacOS_build_preparation\?raw\=1  > my-file.rst``

4. Verify the rst syntax using `./mach lint -l rst`_

.. _./mach lint -l rst: /tools/lint/linters/rstlinter.html
