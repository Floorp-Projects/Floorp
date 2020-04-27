# WebExtension Storage Component

The WebExtension Storage component can be used to power an implementation of the
[`chrome.storage.sync`](https://developer.chrome.com/extensions/storage) WebExtension API,
which gives each WebExtensions its own private key-value store that will sync between a user's
devices. This particular implementation sits atop [Firefox Sync](../sync_manager/README.md).

With a small amount of work, this component would also be capable of powering an implementation
of `chrome.storage.local`, but this is not an explicit goal at this stage.

* [Features](#features)
* [Using the component](#using-the-component)
* [Working on the component](#working-on-the-component)

## Features

The WebExtension Storage component offers:

1. Local storage of key-value data indexed by WebExtension ID.
1. Basic Create, Read, Update and Delete (CRUD) operations for items in the database.
1. Syncing of stored data between applications, via Firefox Sync.

The component ***does not*** offer, but may offer in the future:

1. Separate storage for key-value data that does not sync, per the
   `chrome.storage.local` WebExtension API.
1. Import functionality from previous WebExtension storage implementations backed by
   [Kinto](https://kinto-storage.org).

The component ***does not*** offer, and we have no concrete plans to offer:

1. Any facilities for loading or running WebExtensions, or exposing this data to them.
1. Any helpers to secure data access between different WebExtensions.

As a consuming application, you will need to implement code that plumbs this component in to your
WebExtensions infrastructure, so that each WebExtension gets access to its own data (and only its
own data) stored in this component.

## Using the component

### Prerequisites

To use this component for local storage of WebExtension data, you will need to know how to integrate appservices components
into an application on your target platform:
* **Firefox Desktop**: There's some custom bridging code in mozilla-central.
* **Android**: Bindings not yet available; please reach out on slack to discuss!
* **iOS**: Bindings not yet available; please reach out on slack to discuss!
* **Other Platforms**: We don't know yet; please reach out on slack to discuss!

### Core Concepts

* We assume each WebExtension is uniquely identified by an immutable **extension id**.
* A **WebExtenstion Store** is a database that maps extension ids to key-value JSON maps, one per extension.
  It exposes methods that mirror those of the [`chrome.storage` spec](https://developer.chrome.com/extensions/storage)
  (e.g. `get`, `set`, and `delete`) and which take an extension id as their first argument.

## Working on the component

### Prerequisites

To effectively work on the WebExtension Storage component, you will need to be familiar with:

* Our general [guidelines for contributors](../../docs/contributing.md).
* The [core concepts](#core-concepts) for users of the component, outlined above.
* The way we [generate ffi bindings](../../docs/howtos/building-a-rust-component.md) and expose them to
  [Kotlin](../../docs/howtos/exposing-rust-components-to-kotlin.md) and
  [Swift](../../docs/howtos/exposing-rust-components-to-swift.md).
* The key ideas behind [how Firefox Sync works](../../docs/synconomicon/) and the [sync15 crate](../sync15/README.md).

### Storage Overview

This component stores WebExtension data in a SQLite database, one row per extension id.
The key-value map data for each extension is stored as serialized JSON in a `TEXT` field;
this is nice and simple and helps ensure that the stored data has the semantics we want,
which are pretty much just the semantics of JSON.

For syncing, we maintain a "mirror" table which contains one item per record known to
exist on the server. These items are identified by a randomly-generated GUID, in order
to hide the raw extension ids from the sync server.

When uploading records to the server, we write one
[encrypted BSO](https://mozilla-services.readthedocs.io/en/latest/sync/storageformat5.html#collection-records)
per extension. Its server-visible id is the randomly-generated GUID, and its encrypted payload
contains the plaintext extension id and corresponding key-value map data.

The end result is something like this (highly simplified!) diagram:

[![storage overview diagram](https://docs.google.com/drawings/d/e/2PACX-1vSvCk0uJlXYTtWHmjxhL-mNLGL_q7F50LavltedREH8Ijuqjl875jKYd9PdJ5SrD3mhVOFqANs6A_NB/pub?w=727&h=546)](https://docs.google.com/drawings/d/1MlkFQJ7SUnW4WSEAF9e-2O34EnsAwUFi3Xcf0Lj3Hc8/)

The details of the encryption are handled by the [sync15 crate](../sync15/README.md), following
the formats defied in [sync storage format v5](https://mozilla-services.readthedocs.io/en/latest/sync/storageformat5.html#collection-records).
