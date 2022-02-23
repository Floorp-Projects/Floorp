WebIDL WebExtensions API Bindings
=================================

While on ``manifest_version: 2`` all the extension globals (extension pages and content scripts)
that lives on the main thread and the WebExtensions API bindings can be injected into the extension
global from the JS privileged code part of the WebExtensions internals (`See Schemas.inject defined in
Schemas.jsm <https://searchfox.org/mozilla-central/search?q=symbol:Schemas%23inject&redirect=false>`_),
in ``manifest_version: 3`` the extension will be able to declare a background service worker 
instead of a background page, and the existing WebExtensions API bindings can't be injected into this
new extension global, because it lives off of the main thread.

To expose WebExtensions API bindings to the WebExtensions ``background.service_worker`` global
we are in the process of generating new WebIDL bindings for the WebExtensions API.

.. warning::
   
   TODO: link to dom/webidl docs from this doc page for more general in depth details about WebIDL
   in Gecko.

Background Service Workers API Request Handling
-----------------------------------------------

.. figure:: webidl_bindings_backgroundWorker_apiRequestHandling.drawio.svg
   :alt: High Level Diagram of the Background Service Worker API Request Handling

..
   This svg diagram has been created using https://app.diagrams.net,
   the svg file also includes the source in the drawio format and so
   it can be edited more easily by loading it back into app.diagrams.net
   and then re-export from there (and include the updated drawio format
   content into the exported svg file).

Generating WebIDL definitions from WebExtensions API JSONSchema
---------------------------------------------------------------

WebIDL definitions for the extension APIs are being generated based on the WebExtensions API JSONSchema
data (the same metadata used to generate the "privilged JS"-based API bindings).

Most of the API methods in generated WebIDL are meant to be implemented using stub methods shared
between all WebExtensions API classes, a ``WebExtensionStub`` webidl extended attribute specify
which shared stub method should be used when the related API method is called.

For more in depth details about how to generate or update webidl definition for an Extension API
given its API namespace:

.. toctree::
   :maxdepth: 2

   generate_webidl_from_jsonschema

Wiring up new WebExtensions WebIDL files into mozilla-central
-------------------------------------------------------------

After a new WebIDL definition has been generated, there are a few more steps to ensure that
the new WebIDL binding is wired up into mozilla-central build system and to be able to
complete successfully a full Gecko build that include the new bindings.

For more in depth details about these next steps:

.. toctree::
   :maxdepth: 2

   wiring_up_new_webidl_bindings
