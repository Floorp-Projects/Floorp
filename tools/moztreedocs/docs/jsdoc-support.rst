jsdoc support
=============

ESLint support
--------------

If you are looking to enable JSDoc generation for your documents, please also
enable the ESLint plugin for JSDoc if it is not already enabled.

In the top-level :searchfox:`.eslintrc.js file <.eslintrc.js>` there are currently
two sections where the ``valid-jsdoc`` and ``require-jsdoc`` rules are enabled.
Please check that your component is not excluded from these sections. If it is,
you should remove the exclusion and fix any instances that are raised by running

.. code-block:: shell

    ./mach eslint path/to/component/


Enabling JSDoc generation
-------------------------

Here is a quick example, for the public AddonManager :ref:`API <AddonManager Reference>`

To use it for your own code:

#. Check that JSDoc generates the output you expect (you may need to use a @class annotation on "object initializer"-style class definitions for instance)

#. Create an `.rst file`, which may contain explanatory text as well as the API docs. The minimum will look something like
   `this <https://firefox-source-docs.mozilla.org/_sources/toolkit/mozapps/extensions/addon-manager/AddonManager.rst.txt>`__.

#. Ensure your component is on the js_source_path here in the sphinx
   config: https://hg.mozilla.org/mozilla-central/file/72ee4800d415/tools/docs/conf.py#l46

#. Run `mach doc` locally to generate the output and confirm that it looks correct.
