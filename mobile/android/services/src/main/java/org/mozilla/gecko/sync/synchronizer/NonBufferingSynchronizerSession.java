/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.synchronizer;

import org.mozilla.gecko.sync.repositories.RepositorySession;

public class NonBufferingSynchronizerSession extends SynchronizerSession {
    /* package-private */ NonBufferingSynchronizerSession(Synchronizer synchronizer, SynchronizerSessionDelegate delegate) {
        super(synchronizer, delegate);
    }

    @Override
    protected RecordsChannel getRecordsChannel(RepositorySession sink, RepositorySession source, RecordsChannelDelegate delegate) {
        return new NonBufferingRecordsChannel(sink, source, delegate);
    }
}
