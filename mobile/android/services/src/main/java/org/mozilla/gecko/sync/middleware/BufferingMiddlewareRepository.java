/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.middleware;

import android.content.Context;

import org.mozilla.gecko.sync.SessionCreateException;
import org.mozilla.gecko.sync.middleware.storage.BufferStorage;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.RepositorySession;

/**
 * A buffering-enabled middleware which is intended to wrap local repositories. Configurable with
 * a sync deadline, buffer storage implementation and a consistency checker implementation.
 *
 * @author grisha
 */
public class BufferingMiddlewareRepository extends Repository {
    private final long syncDeadline;
    private final Repository inner;
    private final BufferStorage bufferStorage;

    public BufferingMiddlewareRepository(long syncDeadline, BufferStorage bufferStore, Repository wrappedRepository) {
        this.syncDeadline = syncDeadline;
        this.inner = wrappedRepository;
        this.bufferStorage = bufferStore;
    }

    @Override
    public RepositorySession createSession(Context context) throws SessionCreateException {
        final RepositorySession innerSession = this.inner.createSession(context);
        return new BufferingMiddlewareRepositorySession(innerSession, this, syncDeadline, bufferStorage);
    }
}
