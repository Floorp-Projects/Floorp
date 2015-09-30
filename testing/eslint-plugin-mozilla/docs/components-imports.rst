.. _components-imports:

==================
components-imports
==================

Rule Details
------------

Adds the filename of imported files e.g.
``Cu.import("some/path/Blah.jsm")`` adds Blah to the global scope.

The following patterns are supported:

-  ``Cu.import("resource:///modules/devtools/ViewHelpers.jsm");``
-  ``loader.lazyImporter(this, "name1");``
-  ``loader.lazyRequireGetter(this, "name2"``
-  ``loader.lazyServiceGetter(this, "name3"``
-  ``XPCOMUtils.defineLazyModuleGetter(this, "setNamedTimeout", ...)``
-  ``loader.lazyGetter(this, "toolboxStrings"``
-  ``XPCOMUtils.defineLazyGetter(this, "clipboardHelper"``
