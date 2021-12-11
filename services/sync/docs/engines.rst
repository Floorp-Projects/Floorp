============================
The Sync engines in the tree
============================

Unless otherwise specified, the engine implementations can be found
`here <https://searchfox.org/mozilla-central/source/services/sync/modules/engines>`_

Please read the :doc:`overview`.

Clients
=======

The ``clients`` engine is a special engine in that it's invisible to the
user and can not be disabled - think of it as a "meta" engine. As such, it
doesn't really have a sensible concept of ``store`` or ``tracker``.

The engine is mainly responsible for keeping its own record current in the
``clients`` collection. Some parts of Sync use this collection to know what
other clients exist and when they last synced (although alot of this is moving
to using the Firefox Accounts devices).

Clients also has the ability to handle ``commands`` - in short, some other
client can write to this client's ``commands``, and when this client notices,
it will execute the command. Commands aren't arbitrary, so commands must be
understood by both sides for them to work. There are commands to "wipe"
collections etc. In practice, this is used only by ``bookmarks`` when a device
restores bookmarks - in that case, the restoring device will send a ``wipe``
command to all other clients so that they take the new bookmarks instead of
merging them.

If not for this somewhat limited ``commands`` functionality, this engine could
be considered deprecated and subsumed by FxA devices - but because we
can't just remove support for commands and also do not have a plan for
replacing them, the clients engine remains important.

Bookmarks
=========

The ``bookmarks`` engine has changed so that it's tightly integrated with the
``places`` database. Instead of an external ``tracker``, the tracking is
integrated into Places. Each bookmark has a `syncStatus` and a
`syncChangeCounter` and these are managed internally by places. Sync then just
queries for changed bookmarks by looking for these fields.

Bookmarks is somewhat unique in that it needs to maintain a tree structure,
which makes merging a challenge. The `dogear <https://github.com/mozilla/dogear>`_
component (written in Rust and also used by the
`application-services bookmarks component <https://github.com/mozilla/application-services/tree/main/components/places>`_)
performs this merging.

Bookmarks also pioneered the concept of a "mirror" - this is a database table
which tracks exactly what is on the server. Because each sync only fetches
changes from the server since the last sync, each sync does not supply every
record on the server. However, the merging code does need to know what's on
the server - so the mirror tracks this.

History
=======

History is similar to bookmarks described above - it's closely integrated with
places - but is less complex because there's no tree structure involved.

One unique characteristic of history is that the engine takes steps to *not*
upload everything - old profiles tend to have too much history to reasonably
store and upload, so typically uploads are limited to the  last 5000 visits.

Logins
======

Logins has also been upgraded to be closely integrated with `Services.logins` -
the logins component itself manages the metadata.

Tabs
====

Tabs is a special engine in that there's no underlying storage at all - it
both saves the currently open tabs from this device (which are enumerated
every time it's updated) and also lets other parts of Firefox know which tabs
are open on other devices. There's no database - if we haven't synced yet we
don't know what other tabs are open, and when we do know, the list is just
stored in memory.

The `SyncedTabs module <https://searchfox.org/mozilla-central/source/services/sync/modules/SyncedTabs.jsm>`_
is the main interface the browser uses to get the list of tabs from other
devices.

Add-ons
=======

Addons is still an "old school" engine, with a tracker and store which aren't
closely integrated with the addon manager. As a result it's fairly complex and
error prone - eg, it persists the "last known" state so it can know what to
sync, where a better model would be for the addon manager to track the changes
on Sync's behalf.

It also attempts to sync themes etc. The future of this engine isn't clear given
it doesn't work on mobile platforms.

Addresses / Credit-Cards
========================

Addresses and Credit-cards have Sync functionality tightly bound with the
store. Unlike other engines above, this engine has always been tightly bound,
because it was written after we realized this tight-binding was a feature and
not a bug.

Technically these are 2 separate engines and collections. However, because the
underlying storage uses a shared implementation, the syncing also uses a
shared implementation - ie, the same logic is used for both - so we tend to
treat them as a single engine in practice.

As a result, only a shim is in the `services/sync/modules/engines/` directory,
while the actual logic is
`next to the storage implementation <https://searchfox.org/mozilla-central/source/toolkit/components/formautofill/FormAutofillSync.jsm>`_.

This engine has a unique twist on the "mirror" concept described above -
whenever a change is made to a fields, the original value of the field is
stored directly in the storage. This means that on the next sync, the value
of the record on the server can be deduced, meaning a "3-way" merge can be
done, so it can better tell the difference between local only, remote only, or
conflicting changes.

WebExt-Storage
==============

webext-storage is implemented in Rust and lives in
`application services <https://github.com/mozilla/application-services/tree/main/components/webext-storage>`_
and is vendored into the `addons code <https://searchfox.org/mozilla-central/source/toolkit/components/extensions/storage/webext_storage_bridge>`_ - 
note that this includes the storage *and* Sync code. The Sync engine itself
is a shim in the sync directory.

See the :doc:`rust-engines` document for more about how rust engines are
integrated.
