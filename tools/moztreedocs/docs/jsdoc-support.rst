jsdoc support
=============

Here is a quick example, for the public AddonManager :ref:`API <AddonManager Reference>`

To use it for your own code:

#. Check that JSDoc generates the output you expect (you may need to use a @class annotation on "object initializer"-style class definitions for instance)

#. Create an `.rst file`, which may contain explanatory text as well as the API docs. The minimum will look something like
    `this <https://firefox-source-docs.mozilla.org/_sources/toolkit/mozapps/extensions/addon-manager/AddonManager.rst.txt>`__

#. Ensure your component is on the js_source_path here in the sphinx
    config: https://hg.mozilla.org/mozilla-central/file/72ee4800d415/tools/docs/conf.py#l46

#. Run `mach doc` locally to generate the output and confirm that it looks correct.
