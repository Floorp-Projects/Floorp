========================
How webext storage works
========================

This document describes the implementation of the the `storage.sync` part of the
`WebExtensions Storage APIs
<https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/storage>`_.
The implementation lives in the `toolkit/components/extensions/storage folder <https://searchfox.org/mozilla-central/source/toolkit/components/extensions/storage>`_

Ideally you would already know about Rust and XPCOM - `see this doc for more details <../../../../writing-rust-code/index.html>`_

At a very high-level, the system looks like:

.. mermaid::

    graph LR
        A[Extensions API]
        A --> B[Storage JS API]
        B --> C{magic}
        C --> D[app-services component]

Where "magic" is actually the most interesting part and the primary focus of this document.

    Note: The general mechanism described below is also used for other Rust components from the
    app-services team - for example, "dogear" uses a similar mechanism, and the sync engines
    too (but with even more complexity) to manage the threads. Unfortunately, at time of writing,
    no code is shared and it's not clear how we would, but this might change as more Rust lands.

The app-services component `lives on github <https://github.com/mozilla/application-services/blob/main/components/webext-storage>`_.
There are docs that describe `how to update/vendor this (and all) external rust code <../../../../build/buildsystem/rust.html>`_ you might be interested in.

To set the scene, let's look at the parts exposed to WebExtensions first; there are lots of
moving part there too.

WebExtension API
################

The WebExtension API is owned by the addons team. The implementation of this API is quite complex
as it involves multiple processes, but for the sake of this document, we can consider the entry-point
into the WebExtension Storage API as being `parent/ext-storage.js <https://searchfox.org/mozilla-central/source/toolkit/components/extensions/parent/ext-storage.js>`_

This entry-point ends up using the implementation in the
`ExtensionStorageSync JS class <https://searchfox.org/mozilla-central/rev/9028b0458cc1f432870d2996b186b0938dda734a/toolkit/components/extensions/ExtensionStorageSync.jsm#84>`_.
This class/module has complexity for things like migration from the earlier Kinto-based backend,
but importantly, code to adapt a callback API into a promise based one.

Overview of the API
###################

At a high level, this API is quite simple - there are methods to "get/set/remove" extension
storage data. Note that the "external" API exposed to the addon has subtly changed the parameters
for this "internal" API, so there's an extension ID parameter and the JSON data has already been
converted to a string.
The semantics of the API are beyond this doc but are
`documented on MDN <https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/storage/sync>`_.

As you will see in those docs, the API is promise-based, but the rust implementation is fully
synchronous and Rust knows nothing about Javascript promises - so this system converts
the callback-based API to a promise-based one.

xpcom as the interface to Rust
##############################

xpcom is old Mozilla technology that uses C++ "vtables" to implement "interfaces", which are
described in IDL files. While this traditionally was used to interface
C++ and Javascript, we are leveraging existing support for Rust. The interface we are
exposing is described in `mozIExtensionStorageArea.idl <https://searchfox.org/mozilla-central/source/toolkit/components/extensions/storage/mozIExtensionStorageArea.idl>`_

The main interface of interest in this IDL file is `mozIExtensionStorageArea`.
This interface defines the functionality - and is the first layer in the sync to async model.
For example, this interface defines the following method:

.. code-block:: rust

    interface mozIExtensionStorageArea : nsISupports {
        ...
        // Sets one or more key-value pairs specified in `json` for the
        // `extensionId`...
        void set(in AUTF8String extensionId,
                in AUTF8String json,
                in mozIExtensionStorageCallback callback);

As you will notice, the 3rd arg is another interface, `mozIExtensionStorageCallback`, also
defined in that IDL file. This is a small, generic interface defined as:

.. code-block:: cpp

    interface mozIExtensionStorageCallback : nsISupports {
        // Called when the operation completes. Operations that return a result,
        // like `get`, will pass a `UTF8String` variant. Those that don't return
        // anything, like `set` or `remove`, will pass a `null` variant.
        void handleSuccess(in nsIVariant result);

        // Called when the operation fails.
        void handleError(in nsresult code, in AUTF8String message);
    };

Note that this delivers all results and errors, so must be capable of handling
every result type, which for some APIs may be problematic - but we are very lucky with this API
that this simple XPCOM callback interface is capable of reasonably representing the return types
from every function in the `mozIExtensionStorageArea` interface.

(There's another interface, `mozIExtensionStorageListener` which is typically
also implemented by the actual callback to notify the extension about changes,
but that's beyond the scope of this doc.)

*Note the thread model here is async* - the `set` call will return immediately, and later, on
the main thread, we will call the callback param with the result of the operation.

So under the hood, what happens is something like:

.. mermaid::

    sequenceDiagram
        Extension->>ExtensionStorageSync: call `set` and give me a promise
        ExtensionStorageSync->>xpcom: call `set`, supplying new data and a callback
        ExtensionStorageSync-->>Extension: your promise
        xpcom->>xpcom: thread magic in the "bridge"
        xpcom-->>ExtensionStorageSync: callback!
        ExtensionStorageSync-->>Extension: promise resolved

So onto the thread magic in the bridge!

webext_storage_bridge
#####################

The `webext_storage_bridge <https://searchfox.org/mozilla-central/source/toolkit/components/extensions/storage/webext_storage_bridge>`_
is a Rust crate which, as implied by the name, is a "bridge" between this Javascript/XPCOM world to
the actual `webext-storage <https://github.com/mozilla/application-services/tree/main/components/webext-storage>`_ crate.

lib.rs
------

Is the entry-point - it defines the xpcom "factory function" -
an `extern "C"` function which is called by xpcom to create the Rust object
implementing `mozIExtensionStorageArea` using existing gecko support.

area.rs
-------

This module defines the interface itself. For example, inside that file you will find:

.. code-block:: rust

    impl StorageSyncArea {
        ...

        xpcom_method!(
            set => Set(
                ext_id: *const ::nsstring::nsACString,
                json: *const ::nsstring::nsACString,
                callback: *const mozIExtensionStorageCallback
            )
        );
        /// Sets one or more key-value pairs.
        fn set(
            &self,
            ext_id: &nsACString,
            json: &nsACString,
            callback: &mozIExtensionStorageCallback,
        ) -> Result<()> {
            self.dispatch(
                Punt::Set {
                    ext_id: str::from_utf8(&*ext_id)?.into(),
                    value: serde_json::from_str(str::from_utf8(&*json)?)?,
                },
                callback,
            )?;
            Ok(())
        }


Of interest here:

* `xpcom_method` is a Rust macro, and part of the existing xpcom integration which already exists
  in gecko. It declares the xpcom vtable method described in the IDL.

* The `set` function is the implementation - it does string conversions and the JSON parsing
  on the main thread, then does the work via the supplied callback param, `self.dispatch` and a `Punt`.

* The `dispatch` method dispatches to another thread, leveraging existing in-tree `moz_task <https://searchfox.org/mozilla-central/source/xpcom/rust/moz_task>`_ support, shifting the `Punt` to another thread and making the callback when done.

Punt
----

`Punt` is a whimsical name somewhat related to a "bridge" - it carries things across and back.

It is a fairly simple enum in `punt.rs <https://searchfox.org/mozilla-central/source/toolkit/components/extensions/storage/webext_storage_bridge/src/punt.rs>`_.
It's really just a restatement of the API we expose suitable for moving across threads. In short, the `Punt` is created on the main thread,
then sent to the background thread where the actual operation runs via a `PuntTask` and returns a `PuntResult`.

There's a few dances that go on, but the end result is that `inner_run() <https://searchfox.org/mozilla-central/source/toolkit/components/extensions/storage/webext_storage_bridge/src/punt.rs>`_
gets executed on the background thread - so for `Set`:

.. code-block:: rust

        Punt::Set { ext_id, value } => {
            PuntResult::with_change(&ext_id, self.store()?.get()?.set(&ext_id, value)?)?
        }

Here, `self.store()` is a wrapper around the actual Rust implementation from app-services with
various initialization and mutex dances involved - see `store.rs`.
ie, this function is calling our Rust implementation and stashing the result in a `PuntResult`

The `PuntResult` is private to that file but is a simple struct that encapsulates both
the actual result of the function (also a set of changes to send to observers, but that's
beyond this doc).

Ultimately, the `PuntResult` ends up back on the main thread once the call is complete
and arranges to callback the JS implementation, which in turn resolves the promise created in `ExtensionStorageSync.jsm`

End result:
-----------

.. mermaid::

    sequenceDiagram
        Extension->>ExtensionStorageSync: call `set` and give me a promise
        ExtensionStorageSync->>xpcom - bridge main thread: call `set`, supplying new data and a callback
        ExtensionStorageSync-->>Extension: your promise
        xpcom - bridge main thread->>moz_task worker thread: Punt this
        moz_task worker thread->>webext-storage: write this data to the database
        webext-storage->>webext-storage: done: result/error and observers
        webext-storage-->>moz_task worker thread: ...
        moz_task worker thread-->>xpcom - bridge main thread: PuntResult
        xpcom - bridge main thread-->>ExtensionStorageSync: callback!
        ExtensionStorageSync-->>Extension: promise resolved
