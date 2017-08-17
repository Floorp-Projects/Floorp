/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.stage;

import android.net.Uri;
import android.os.Bundle;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.db.BrowserContract;

/**
 * Server sync stage which knows how to reset record versions for its local repository.
 */
/* package-private */  abstract class VersionedServerSyncStage extends ServerSyncStage {
    /* package-private */ abstract Uri getLocalDataUri();

    @Override
    protected void resetLocal() {
        super.resetLocal();

        final Bundle result = session.getContext().getContentResolver().call(
                getLocalDataUri(),
                BrowserContract.METHOD_RESET_RECORD_VERSIONS,
                getLocalDataUri().toString(),
                null
        );

        if (result == null) {
            Logger.error(LOG_TAG, "Failed to get a result bundle while resetting record versions for " + getCollection());
        } else {
            final int recordsChanged = (int) result.getSerializable(BrowserContract.METHOD_RESULT);
            Logger.info(LOG_TAG, "Reset versions for records in " + getCollection() + ": " + recordsChanged);
        }
    }
}
