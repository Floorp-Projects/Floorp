Importing documentation from MDN
--------------------------------

As MDN should not be used for documenting mozilla-central specific code or process,
the documentation should be migrated in this repository.

The meta bug is `Bug 1617963 <https://bugzilla.mozilla.org/show_bug.cgi?id=migrate-from-mdn>`__.

Fortunately, there is an easy way to import the doc from MDN using GitHub
to the firefox source docs.

1. Install https://pandoc.org/ - If you are using packages provided by your distribution,
   make sure that the version is not too old.

2. Identify where your page is located on the GitHub repository ( https://github.com/mdn/archived-content/tree/main/files/en-us/mozilla ).
   Get the raw URL

3. Run pandoc the following way:

.. code-block:: shell

   $ pandoc -t rst https://github.com/mdn/archived-content/tree/main/files/en-us/mozilla/firefox/performance_best_practices_for_firefox_fe_engineers > doc.rst

4. In the new doc.rst, identify the images and wget/curl them into `img/`.

5. Verify the rst syntax using `./mach lint -l rst`_

.. _./mach lint -l rst: /tools/lint/linters/rstlinter.html

6. If relevant, remove unbreakable spaces (rendered with a "!" on Phabricator)

.. code-block:: shell

   $ sed -i -e 's/\xc2\xa0/ /g' doc.rst
