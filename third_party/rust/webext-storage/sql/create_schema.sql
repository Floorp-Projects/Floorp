-- This Source Code Form is subject to the terms of the Mozilla Public
-- License, v. 2.0. If a copy of the MPL was not distributed with this
-- file, You can obtain one at http://mozilla.org/MPL/2.0/.

-- This is a very simple schema for a chrome.storage.* implementation. At time
-- of writing, only chrome.storage.sync is supported, but this can be trivially
-- enhanced to support chrome.storage.local (the api is identical, it's just a
-- different "bucket" and doesn't sync).
--
-- Even though the spec allows for a single extension to have any number of
-- "keys", we've made the decision to store all keys for a given extension in a
-- single row as a JSON representation of all keys and values.
-- We've done this primarily due to:
-- * The shape of the API is very JSON, and it almost encourages multiple keys
--   to be fetched at one time.
-- * The defined max sizes that extensions are allowed to store using this API
--   is sufficiently small that we don't have many concerns around record sizes.
-- * We'd strongly prefer to keep one record per extension when syncing this
--   data, so having the local store in this shape makes syncing easier.

CREATE TABLE IF NOT EXISTS storage_sync_data (
    ext_id TEXT NOT NULL PRIMARY KEY,

    /* The JSON payload. NULL means it's a tombstone */
    data TEXT,

    /* Same "sync change counter" strategy used by other components. */
    sync_change_counter INTEGER NOT NULL DEFAULT 1
);

CREATE TABLE IF NOT EXISTS storage_sync_mirror (
    guid TEXT NOT NULL PRIMARY KEY,

    /* The extension_id is explicitly not the GUID used on the server.
       It can't be  a regular foreign-key relationship back to storage_sync_data
       as items with no data on the server (ie, deleted items) will not appear
       in storage_sync_data, and the guid isn't in that table either.
       It must allow NULL as tombstones do not carry the ext_id, so we have
       an additional CHECK constraint.
    */
    ext_id TEXT UNIQUE,

    /* The JSON payload. We *do* allow NULL here - it means "deleted" */
    data TEXT

    /* tombstones have no ext_id and no data. Non tombstones must have both */
    CHECK((ext_id IS NULL AND data IS NULL) OR (ext_id IS NOT NULL AND data IS NOT NULL))
);

-- This table holds key-value metadata - primarily for sync.
CREATE TABLE IF NOT EXISTS meta (
    key TEXT PRIMARY KEY,
    value NOT NULL
) WITHOUT ROWID;
