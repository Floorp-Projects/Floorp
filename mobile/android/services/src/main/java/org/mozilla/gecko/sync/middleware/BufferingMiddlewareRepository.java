/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.middleware;

import android.content.Context;

import org.mozilla.gecko.sync.middleware.storage.BufferStorage;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionCreationDelegate;

/**
 * A buffering-enabled middleware which is intended to wrap local repositories. Configurable with
 * a sync deadline, buffer storage implementation and a consistency checker implementation.
 *
 * @author grisha
 */
public class BufferingMiddlewareRepository extends MiddlewareRepository {
    private final long syncDeadline;
    private final Repository inner;
    private final BufferStorage bufferStorage;

    private class BufferingMiddlewareRepositorySessionCreationDelegate extends MiddlewareRepository.SessionCreationDelegate {
        private final BufferingMiddlewareRepository repository;
        private final RepositorySessionCreationDelegate outerDelegate;

        private BufferingMiddlewareRepositorySessionCreationDelegate(BufferingMiddlewareRepository repository, RepositorySessionCreationDelegate outerDelegate) {
            this.repository = repository;
            this.outerDelegate = outerDelegate;
        }

        @Override
        public void onSessionCreateFailed(Exception ex) {
            this.outerDelegate.onSessionCreateFailed(ex);
        }

        @Override
        public void onSessionCreated(RepositorySession session) {
            outerDelegate.onSessionCreated(new BufferingMiddlewareRepositorySession(
                    session, this.repository, syncDeadline, bufferStorage
            ));
        }
    }

    public BufferingMiddlewareRepository(long syncDeadline, BufferStorage bufferStore, Repository wrappedRepository) {
        this.syncDeadline = syncDeadline;
        this.inner = wrappedRepository;
        this.bufferStorage = bufferStore;
    }

    @Override
    public void createSession(RepositorySessionCreationDelegate delegate, Context context) {
        this.inner.createSession(
                new BufferingMiddlewareRepositorySessionCreationDelegate(this, delegate),
                context
        );
    }
}
