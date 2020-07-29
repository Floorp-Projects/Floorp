.. _basics:

API Implementation Basics
=========================
This page describes some of the pieces involved when creating
WebExtension APIs.  Detailed documentation about how these pieces work
together to build specific features is in the next section.

The API Schema
--------------
As described previously, a WebExtension runs in a sandboxed environment
but the implementation of a WebExtension API runs with full chrome
privileges.  API implementations do not directly interact with
extensions' Javascript environments, that is handled by the WebExtensions
framework.  Each API includes a schema that describes all the functions,
events, and other properties that the API might inject into an
extension's Javascript environment.  
Among other things, the schema specifies the namespace into which
an API should be injected, what permissions (if any) are required to
use it, and in which contexts (e.g., extension pages, content scripts, etc)
it should be available.  The WebExtensions framework reads this schema
and takes care of injecting the right objects into each extension
Javascript environment.

API schemas are written in JSON and are based on
`JSON Schema <http://json-schema.org/>`_ with some extensions to describe
API functions and events.
The next section describes the format of the schema in detail.

The ExtensionAPI class
----------------------
Every WebExtension API is represented by an instance of the Javascript
`ExtensionAPI <reference.html#extensionapi-class>`_ class.
An instance of its API class is created every time an extension that has
access to the API is enabled.  Instances of this class contain the
implementations of functions and events that are exposed to extensions,
and they also contain code for handling manifest keys as well as other
part of the extension lifecycle (e.g., updates, uninstalls, etc.)
The details of this class are covered in a subsequent section, for now the
important point is that this class contains all the actual code that
backs a particular WebExtension API.

Built-in APIs versus Experiments
--------------------------------
A WebExtension API can be built directly into the browser or it can be
contained in a special type of extension called a "WebExtension Experiment".
The API schema and the ExtensionAPI class are written in the same way
regardless of how the API will be delivered, the rest of this section
explains how to package a new API using these methods.

Adding a built-in API
---------------------
Built-in WebExtension APIs are loaded lazily.  That is, the schema and
accompanying code are not actually loaded and interpreted until an
extension that uses the API is activated.
To actually register the API with the WebExtensions framework, an entry
must be added to the list of WebExtensions modules in one of the following
files:

- ``toolkit/components/extensions/ext-toolkit.json``
- ``browser/components/extensions/ext-browser.json``
- ``mobile/android/components/extensions/ext-android.json``

Here is a sample fragment for a new API:

.. code-block:: js

    "myapi": {
      "schema": "chrome://extensions/content/schemas/myapi.json",
      "url": "chrome://extensions/content/ext-myapi.js",
      "paths": [
        ["myapi"],
        ["anothernamespace", "subproperty"]
      ],
      "scopes": ["addon_parent"],
      "permissions": ["myapi"],
      "manifest": ["myapi_key"],
      "events": ["update", "uninstall"]
    }

The ``schema`` and ``url`` properties are simply URLs for the API schema
and the code implementing the API.  The ``chrome:`` URLs in the example above
are typically created by adding entries to ``jar.mn`` in the mozilla-central
directory where the API implementation is kept.  The standard locations for
API implementations are:

- ``toolkit/components/extensions``: This is where APIs that work in both
  the desktop and mobile versions of Firefox (as well as potentially any
  other applications built on Gecko) should go
- ``browser/components/extensions``: APIs that are only supported on
  Firefox for the desktop.
- ``mobile/android/components/extensions``: APIs that are only supported
  on Firefox for Android.

Within the appropriate extensions directory, the convention is that the
API schema is in a file called ``schemas/name.json`` (where *name* is
the name of the API, typically the same as its namespace if it has
Javascript visible features).  The code for the ExtensionAPI class is put
in a file called ``ext-name.js``.  If the API has code that runs in a
child process, that is conventionally put in a file called ``ext-c-name.js``.

The remaining properties specify when an API should be loaded.
The ``paths``, ``scopes``, and ``permissions`` properties together
cause an API to be loaded when Javascript code in an extension references
something beneath the ``browser`` global object that is part of the API.
The ``paths`` property is an array of paths where each individual path is
also an array of property names.  In the example above, the sample API will
be loaded if an extension references either ``browser.myapi`` or
``browser.anothernamespace.subproperty``.

A reference to a property beneath ``browser`` only causes the API to be
loaded if it occurs within a scope listed in the ``scopes`` property.
A scope corresponds to the combination of a Javascript environment
(e.g., extension pages, content scripts, etc) and the process in which the
API code should run (which is either the main/parent process, or a
content/child process).
Valid ``scopes`` are:

- ``"addon_parent"``, ``"addon_child``: Extension pages

- ``"content_parent"``, ``"content_child``: Content scripts

- ``"devtools_parent"``, ``"devtools_child"``: Devtools pages

The distinction between the ``_parent`` and ``_child`` scopes will be
explained in further detail in following sections.

A reference to a property only causes the API to be loaded if the
extension referencing the property also has all the permissions listed
in the ``permissions`` property.

A WebExtension API that is controlled by a manifest key can also be loaded
when an extension that includes the relevant manifest key is activated.
This is specified by the ``manifest`` property, which lists any manifest keys
that should cause the API to be loaded.

Finally, APIs can be loaded based on other events in the WebExtension
lifecycle.  These are listed in the ``events`` property and described in
more detail in :ref:`lifecycle`.

WebExtensions Experiments
-------------------------
A new API may also be implemented within an extension. An API implemented
this way is called a WebExtension Experiment.  Experiments can be useful
when actively developing a new API, as they do not require building
Firefox locally. Note that extensions that include experiments cannot be
signed by addons.mozilla.org.  They may be installed temporarily via
``about:debugging`` or, on browser that support it (current Nightly and
Developer Edition), by setting the preference
``xpinstall.signatures.required`` to ``false``.  You may also set the
preference ``extensions.experiments.enabled`` to ``true`` to install the
addon normally and test across restart.

Experimental APIs have a few limitations compared with built-in APIs:

- Experimental APIs can (currently) only be exposed to extension pages,
  not to devtools pages or to content scripts.
- Experimental APIs cannot handle manifest keys (since the extension manifest
  needs to be parsed and validated before experimental APIs are loaded).
- Experimental APIs cannot use the static ``"update"`` and ``"uninstall"``
  lifecycle events (since in general those may occur when an affected
  extension is not active or installed).

Experimental APIs are declared in the ``experiment_apis`` property in a
WebExtension's ``manifest.json`` file.  For example:

.. code-block:: js

  {
    "manifest_version": 2,
    "name": "Extension containing an experimental API",
    "experiment_apis": {
      "apiname": {
        "schema": "schema.json",
        "parent": {
          "scopes": ["addon_parent"],
          "paths": [["myapi"]],
          "script": "implementation.js"
        },

        "child": {
          "scopes": ["addon_child"],
          "paths": [["myapi"]],
          "script": "child-implementation.js"
        }
      }
    }
  }

This is essentially the same information required for built-in APIs,
just organized differently.  The ``schema`` property is a relative path
to a file inside the extension containing the API schema.  The actual
implementation details for the parent process and for child processes
are defined in the ``parent`` and ``child`` properties of the API
definition respectively.  Inside these sections, the ``scope`` and ``paths``
properties have the same meaning as those properties in the definition
of a built-in API (though see the note above about limitations; the
only currently valid values for ``scope`` are ``"addon_parent"`` and
``"addon_child"``).  The ``script`` property is a relative path to a file
inside the extension containing the implementation of the API.

The extension that includes an experiment defined in this way automatically
gets access to the experimental API.  An extension may also use an
experimental API implemented in a different extension by including the
string ``experiments.name`` in the ``permissions``` property in its
``manifest.json`` file.  In this case, the string name must be replace by
the name of the API from the extension that defined it (e.g., ``apiname``
in the example above.
