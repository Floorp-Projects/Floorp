/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.stage;

import org.mozilla.gecko.sync.MetaGlobalException;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.sync.SynchronizerConfiguration;
import org.mozilla.gecko.sync.middleware.BufferingMiddlewareRepository;
import org.mozilla.gecko.sync.middleware.storage.MemoryBufferStorage;
import org.mozilla.gecko.sync.repositories.ConstrainedServer11Repository;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.android.AndroidBrowserHistoryRepository;

import java.io.IOException;
import java.net.URISyntaxException;

/**
 * History sync stage which is limited to just recent history, and will only run if the full history
 * sync stage did not complete yet. Its purpose is to give users with a lot of history in their
 * profiles a good experience during a large collection sync.
 *
 * @author grisha
 */
public class AndroidBrowserRecentHistoryServerSyncStage extends AndroidBrowserHistoryServerSyncStage {
    protected static final String LOG_TAG = "RecentHistoryStage";

    // TODO: Bug 1316110 tracks follow up work to make this stage more efficient.
    private static final int HISTORY_BATCH_LIMIT = 50;
    // We need a custom configuration bundle name for this stage, because we want to track last-synced
    // timestamp for this stage separately from that of a full history sync stage, yet their collection
    // names are the same.
    private static final String BUNDLE_NAME = "recentHistory.";
    private static final String HISTORY_SORT = "newest";

    @Override
    public String bundlePrefix() {
        return BUNDLE_NAME;
    }

    @Override
    protected Repository getLocalRepository() {
        return new BufferingMiddlewareRepository(
                session.getSyncDeadline(),
                new MemoryBufferStorage(),
                new AndroidBrowserHistoryRepository()
        );
    }

    @Override
    protected Repository getRemoteRepository() throws URISyntaxException {
        return new ConstrainedServer11Repository(
                getCollection(),
                session.getSyncDeadline(),
                session.config.storageURL(),
                session.getAuthHeaderProvider(),
                session.config.infoCollections,
                session.config.infoConfiguration,
                HISTORY_BATCH_LIMIT,
                HISTORY_SORT,
                false /* force single batch only */);
    }

    /**
     * This stage is only enabled if full history session is enabled and did not complete a sync yet.
     */
    @Override
    public boolean isEnabled() throws MetaGlobalException {
        final boolean historyStageEnabled = super.isEnabled();
        if (!historyStageEnabled) {
            return false;
        }

        if (session.config == null) {
            return false;
        }

        final SynchronizerConfiguration synchronizerConfiguration;
        try {
            synchronizerConfiguration = new SynchronizerConfiguration(session.config.getBranch(getCollection() + "."));
        } catch (IOException|NonObjectJSONException e) {
            return false;
        }

        return synchronizerConfiguration.localBundle.getTimestamp() == -1;
    }
}
