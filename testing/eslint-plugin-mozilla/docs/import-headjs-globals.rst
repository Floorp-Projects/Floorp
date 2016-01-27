.. _import-headjs-globals:

=====================
import-headjs-globals
=====================

Rule Details
------------

Import globals from head.js and from any files that were imported by
head.js (as far as we can correctly resolve the path).

The following file import patterns are supported:

-  ``Services.scriptloader.loadSubScript(path)``
-  ``loader.loadSubScript(path)``
-  ``loadSubScript(path)``
-  ``loadHelperScript(path)``
-  ``import-globals-from path``

If path does not exist because it is generated e.g.
``testdir + "/somefile.js"`` we do our best to resolve it.

The following patterns are supported:

-  ``Cu.import("resource://devtools/client/shared/widgets/ViewHelpers.jsm");``
-  ``loader.lazyImporter(this, "name1");``
-  ``loader.lazyRequireGetter(this, "name2"``
-  ``loader.lazyServiceGetter(this, "name3"``
-  ``XPCOMUtils.defineLazyModuleGetter(this, "setNamedTimeout", ...)``
-  ``loader.lazyGetter(this, "toolboxStrings"``
-  ``XPCOMUtils.defineLazyGetter(this, "clipboardHelper"``
