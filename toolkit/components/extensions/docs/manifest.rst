Implementing a manifest property
================================
Like functions and events, implementing a new manifest key requires
writing a definition in the schema and extending the API's instance
of ``ExtensionAPI``.

The contents of a WebExtension's ``manifest.json`` are validated using
a type called ``WebExtensionManifest`` defined in the namespace
``manifest``.
The first step when adding a new property is to extend the schema so
that manifests containing the new property pass validation.
This is done with the ``"$extend"`` property as follows:

.. code-block:: json

   [
     "namespace": "manifest",
     "types": [
       {
         "$extend": "WebExtensionManifest",
         "properties": {
           "my_api_property": {
             "type": "string",
             "optional": true,
             ...
           }
         }
       }
     ]
   ]

The next step is to inform the WebExtensions framework that this API
should be instantiated and notified when extensions that use the new
manifest key are loaded.
For built-in APIs, this is done with the ``manifest`` property
in the API manifest (e.g., ``ext-toolkit.json``).
Note that this property is an array so an extension can implement
multiple properties:

.. code-block:: json

   "myapi": {
     "schema": "...",
     "url": "...",
     "manifest": ["my_api_property"]
   }

The final step is to write code to handle the new manifest entry.
The WebExtensions framework processes an extension's manifest when the
extension starts up, this happens for existing extensions when a new
browser session starts up and it can happen in the middle of a session
when an extension is first installed or enabled, or when the extension
is updated.
The JSON fragment above causes the WebExtensions framework to load the
API implementation when it encounters a specific manifest key while
starting an extension, and then call its ``onManifestEntry()`` method
with the name of the property as an argument.
The value of the property is not passed, but the full manifest is
available through ``this.extension.manifest``:

.. code-block:: js

   class myapi extends ExtensionAPI {
     onManifestEntry(name) {
       let value = this.extension.manifest.my_api_property;
       /* do something with value... */
     }
   }
