.. _lifecycle:

Managing the Extension Lifecycle
================================
The techniques described in previous pages allow a WebExtension API to
be loaded and instantiated only when an extension that uses the API is
activated.
But there are a few other events in the extension lifecycle that an API
may need to respond to.

Extension Shutdown
------------------
APIs that allocate any resources (e.g., adding elements to the browser's
user interface, setting up internal event listeners, etc.) must free
these resources when the extension for which they are allocated is
shut down.  An API does this by using the ``callOnClose()``
method on an `Extension <reference.html#extension-class>`_ object. 

Extension Uninstall and Update
------------------------------
In addition to resources allocated within an individual browser session,
some APIs make durable changes such as setting preferences or storing
data in the user's profile.
These changes are typically not reverted when an extension is shut down,
but when the extension is completely uninstalled (or stops using the API).
To handle this, extensions can be notified when an extension is uninstalled
or updated.  Extension updates are a subtle case -- consider an API that
makes some durable change based on the presence of a manifest property.
If an extension uses the manifest key in one version and then is updated
to a new version that no longer uses the manifest key,
the ``onManifestEntry()`` method for the API is no longer called,
but an API can examine the new manifest after an update to detect that
the key has been removed.

To be notified of update and uninstall events, an extension lists these
events in the API manifest:

.. code-block:: json

   "myapi": {
     "schema": "...",
     "url": "...",
     "events": ["update", "uninstall"]
   }

If these properties are present, the ``onUpdate()`` and ``onUninstall()``
methods will be called for the relevant ``ExtensionAPI`` instances when
an extension that uses the API is updated or uninstalled.

.. Should we even document onStartup()?  I think no...
