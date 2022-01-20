================================
How Rust Engines are implemented
================================

There are 2 main components to engines implemented in Rust

The bridged-engine
==================

Because Rust engines still need to work with the existing Sync infrastructure,
there's the concept of a `bridged-engine <https://searchfox.org/mozilla-central/source/services/sync/modules/bridged_engine.js>`_.
In short, this is just a shim between the existing
`Sync Service <https://searchfox.org/mozilla-central/source/services/sync/modules/service.js>`_
and the Rust code.

The bridge
==========

`"Golden Gate" <https://searchfox.org/mozilla-central/source/services/sync/golden_gate>`_
is a utility to help bridge any Rust implemented Sync engines with desktop. In
other words, it's a "rusty bridge" - get it? Get it? Yet another of Lina's puns
that live on!

One of the key challenges with integrating a Rust Sync component with desktop
is the different threading models. The Rust code tends to be synchronous -
most functions block the calling thread to do the disk or network IO necessary
to work - it assumes that the consumer will delegate this to some other thread.

So golden_gate is this background thread delegation for a Rust Sync engine -
gecko calls golden-gate on the main thread, it marshalls the call to a worker
thread, and the result is marshalled back to the main thread.

It's worth noting that golden_gate is just for the Sync engine part - other
parts of the component (ie, the part that provides the functionality that's not
sync related) will have its own mechanism for this. For example, the
`webext-storage bridge <https://searchfox.org/mozilla-central/source/toolkit/components/extensions/storage/webext_storage_bridge/src>`_
uses a similar technique `which has some in-depth documentation <../../toolkit/components/extensions/webextensions/webext-storage.html>`_.
