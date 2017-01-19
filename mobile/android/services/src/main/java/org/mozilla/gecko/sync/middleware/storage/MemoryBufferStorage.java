/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.middleware.storage;

import org.mozilla.gecko.sync.repositories.domain.Record;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/**
 * A trivial, memory-backed, transient implementation of a BufferStorage.
 * Its intended use is to buffer syncing of small collections.
 * Thread-safe.
 *
 * @author grisha
 */
public class MemoryBufferStorage implements BufferStorage {
    private final Map<String, Record> recordBuffer = Collections.synchronizedMap(new HashMap<String, Record>());

    @Override
    public boolean isPersistent() {
        return false;
    }

    @Override
    public Collection<Record> all() {
        synchronized (recordBuffer) {
            return new ArrayList<>(recordBuffer.values());
        }
    }

    @Override
    public void addOrReplace(Record record) {
        recordBuffer.put(record.guid, record);
    }

    @Override
    public void flush() {
        // This is a no-op; flush intended for database-backed stores.
    }

    @Override
    public void clear() {
        recordBuffer.clear();
    }
}
