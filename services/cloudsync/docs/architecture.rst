.. _cloudsync_architecture:

============
Architecture
============

CloudSync offers functionality similar to Firefox Sync for data sources. Third-party addons
(sync adapters) consume local data, send and receive updates from the cloud, and merge remote data.


Files
=====

CloudSync.jsm
    Main module; Includes other modules and exposes them.

CloudSyncAdapters.jsm
    Provides an API for addons to register themselves. Will be used to
    list available adapters and to notify adapters when sync operations
    are requested manually by the user.

CloudSyncBookmarks.jsm
    Provides operations for interacting with bookmarks.

CloudSyncBookmarksFolderCache.jsm
    Implements a cache used to store folder hierarchy for filtering bookmark events.

CloudSyncEventSource.jsm
    Implements an event emitter. Used to provide addEventListener and removeEventListener
    for tabs and bookmarks.

CloudSyncLocal.jsm
    Provides information about the local device, such as name and a unique id.

CloudSyncPlacesWrapper.jsm
    Wraps parts of the Places API in promises. Some methods are implemented to be asynchronous
    where they are not in the places API.

CloudSyncTabs.jsm
    Provides operations for fetching local tabs and for populating the about:sync-tabs page.


Data Sources
============

CloudSync provides data for tabs and bookmarks. For tabs, local open pages can be enumerated and
remote tabs can be merged for displaying in about:sync-tabs. For bookmarks, updates are tracked
for a named folder (given by each adapter) and handled by callbacks registered using addEventListener,
and remote changes can be merged into the local database.

Versioning
==========

The API carries an integer version number (clouySync.version). Data records are versioned separately and individually.
