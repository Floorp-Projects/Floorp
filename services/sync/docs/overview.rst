====================
Introduction to Sync
====================

This document is a brief introduction to how Sync is implemented in desktop Firefox.

General, Historical, Anatomy of a Sync Engine
=============================================

This section describes how Sync used to work - and indeed, how much of it still
does. While we discuss how this is slowly changing, this context is valuable.

For any datatype which syncs, there tends to be 3 parts:

Store
-----

The sync ``store`` interfaces with the actual Firefox desktop store. For example,
in the ``passwords`` engine, the "store" is that layer that talks to
``Services.logins``

Tracker
-------

The ``tracker`` is what knows that something should be synced. For example,
when the user creates or updates a password, it is the tracker that knows
we should sync now, and what particular password(s) should be updated.

This is typically done via "observer" notifications - ``Services.logins``,
``places`` etc all send specific notifications when certain events happen
(and indeed, some of these were added for Sync's benefit)

Engine
------

The ``engine`` ties it all together. It works with the ``store`` and
``tracker`` and tracks its own metadata (eg, the timestamp of the passwords on
the server, so it knows how to grab just changed records and how to pass them
off to the ``store`` so the actual underlying storage can be updated.

All of the above parts were typically in the
`services/sync/modules/engines directory <https://searchfox.org/mozilla-central/source/services/sync/modules/engines>`_
directory and decoupled from the data they were syncing.


The Future of Desktop-Specific Sync Engines
===========================================

The system described above reflects the fact that Sync was "bolted on" to
Desktop Firefox relatively late - eg, the Sync ``store`` is decoupled from the
actual ``store``. This has causes a number of problems - particularly around
the ``tracker`` and the metadata used by the engine, and the fact that changes
to the backing store would often forget that Sync existed.

Over the last few years, the Sync team has come to the conclusion that Sync
support must be integrated much closer to the store itself. For example,
``Services.logins`` should track when something has changed that would cause
an item to be synced. It should also track the metadata for the store so that
if (say) a corrupt database is recovered by creating a new, empty one, the
metadata should also vanish so Sync knows something bad has happened and can
recover.

However, this is a slow process - currently the ``bookmarks``, ``history`` and
``passwords`` legacy engines have been improved so more responsibility is taken
by the stores. In all cases, for implementation reasons, the Sync
implementation still has a ``store``, but it tends to be a thin wrapper around
the actual underlying store.

The Future of Cross-Platform Sync Engines
=========================================

There are a number of Sync engines implemented in Rust and which live in the
application-services repository. While these were often done for mobile
platforms, the longer term hope is that they can be reused on Desktop.
:doc:`engines` has more details on these.

While no existing engines have been replaced with Rust implemented engines,
the webext-storage engine is implemented in Rust via application-services, so
doesn't tend to use any of the infrastructure described above.

Hopefully over time we will find more Rust-implemented engines in Desktop.
