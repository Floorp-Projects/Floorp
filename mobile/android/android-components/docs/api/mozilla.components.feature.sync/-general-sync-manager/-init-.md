[android-components](../../index.md) / [mozilla.components.feature.sync](../index.md) / [GeneralSyncManager](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`GeneralSyncManager()`

A base SyncManager implementation which manages a dispatcher, handles authentication and requests
synchronization in the following manner:
On authentication AND on store set changes (add or remove)...

* immediate sync and periodic syncing are requested
On logout...
* periodic sync is requested to stop
