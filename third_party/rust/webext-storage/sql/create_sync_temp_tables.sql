-- This Source Code Form is subject to the terms of the Mozilla Public
-- License, v. 2.0. If a copy of the MPL was not distributed with this
-- file, You can obtain one at http://mozilla.org/MPL/2.0/.

-- Temp tables used by Sync.
-- Note that this is executed both before and after a sync.

CREATE TEMP TABLE IF NOT EXISTS storage_sync_staging (
    guid TEXT NOT NULL PRIMARY KEY,

    /* The extension_id is explicitly not the GUID used on the server.
       It can't be  a regular foreign-key relationship back to storage_sync_data,
       nor can it be NOT NULL, as the ext_id for incoming items may not appear
       in storage_sync_data at the time we populate this table, and also
       because incoming tombstones have no extension ID.
    */
    ext_id TEXT UNIQUE,

    /* The JSON payload. We *do* allow NULL here - it means "deleted" */
    data TEXT
);

DELETE FROM temp.storage_sync_staging;

-- We record the changes we are making via sync in this table, so that at the
-- end of the sync extensions can find out via notifications what changes
-- were applied.
CREATE TEMP TABLE IF NOT EXISTS storage_sync_applied (
    ext_id TEXT NOT NULL UNIQUE,

    /* A StorageChanges value serialized as JSON. */
    changes TEXT NOT NULL
);

DELETE FROM temp.storage_sync_applied;

-- We store metadata about items we are uploading in this temp table. After
-- we get told the upload was successful we use this to update the local
-- tables.
CREATE TEMP TABLE IF NOT EXISTS storage_sync_outgoing_staging (
    guid TEXT NOT NULL PRIMARY KEY DEFAULT(generate_guid()),
    ext_id TEXT NOT NULL UNIQUE,
    data TEXT,
    sync_change_counter INTEGER NOT NULL,
    was_uploaded BOOLEAN NOT NULL DEFAULT 0
);

CREATE TEMP TRIGGER IF NOT EXISTS record_uploaded
AFTER UPDATE OF was_uploaded ON storage_sync_outgoing_staging
WHEN NEW.was_uploaded
BEGIN
    -- Decrement the local change counter for uploaded items. If any local items
    -- changed while we were uploading, their change counters will remain > 0,
    -- and we'll merge them again on the next sync.
    UPDATE storage_sync_data SET
      sync_change_counter = sync_change_counter - NEW.sync_change_counter
    WHERE NEW.ext_id IS NOT NULL AND ext_id = NEW.ext_id;

    -- Delete uploaded tombstones entirely; they're only kept in the mirror.
    DELETE FROM storage_sync_data WHERE data IS NULL AND sync_change_counter = 0 AND ext_id = NEW.ext_id;

    -- And write the uploaded item back to the mirror.
    INSERT OR REPLACE INTO storage_sync_mirror (guid, ext_id, data)
    -- Our mirror has a constraint for tombstones, so handle that - if data is
    -- null we want a null ext_id (as that's whats on the server)
    VALUES (NEW.guid, CASE WHEN NEW.data IS NULL THEN NULL ELSE NEW.ext_id END, NEW.data);
END;

DELETE FROM temp.storage_sync_outgoing_staging;
