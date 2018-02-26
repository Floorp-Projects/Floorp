/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.stage;

import org.mozilla.gecko.sync.synchronizer.NonBufferingSynchronizer;
import org.mozilla.gecko.sync.synchronizer.Synchronizer;

public abstract class NonBufferingServerSyncStage extends ServerSyncStage {
    @Override
    protected Synchronizer getSynchronizer() {
        return new NonBufferingSynchronizer();
    }
}
