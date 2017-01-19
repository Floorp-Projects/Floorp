/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.middleware.storage;

import org.mozilla.gecko.sync.repositories.domain.Record;

import java.util.Collection;

/**
 * A contract between BufferingMiddleware and specific storage implementations.
 *
 * @author grisha
 */
public interface BufferStorage {
    // Returns all of the records currently present in the buffer.
    Collection<Record> all();

    // Implementations are responsible to ensure that any incoming records with duplicate GUIDs replace
    // what's already present in the storage layer.
    // NB: For a database-backed storage, "replace" happens at a transaction level.
    void addOrReplace(Record record);

    // For database-backed implementations, commits any records that came in up to this point.
    void flush();

    void clear();

    // For buffers that are filled up oldest-first this is a high water mark, which enables resuming
    // a sync.
    long latestModifiedTimestamp();

    boolean isPersistent();
}
