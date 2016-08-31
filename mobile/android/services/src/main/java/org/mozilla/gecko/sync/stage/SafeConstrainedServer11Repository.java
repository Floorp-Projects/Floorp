/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.stage;

import java.net.URISyntaxException;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.InfoCollections;
import org.mozilla.gecko.sync.InfoConfiguration;
import org.mozilla.gecko.sync.InfoCounts;
import org.mozilla.gecko.sync.JSONRecordFetcher;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.repositories.ConstrainedServer11Repository;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.Server11RepositorySession;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionCreationDelegate;

import android.content.Context;

/**
 * This is a constrained repository -- one which fetches a limited number
 * of records -- that additionally refuses to sync if the limit will
 * be exceeded on a first sync by the number of records on the server.
 *
 * You must pass an {@link InfoCounts} instance, which will be interrogated
 * in the event of a first sync.
 *
 * "First sync" means that our sync timestamp is not greater than zero.
 */
public class SafeConstrainedServer11Repository extends ConstrainedServer11Repository {

  // This can be lazily evaluated if we need it.
  private final JSONRecordFetcher countFetcher;

  public SafeConstrainedServer11Repository(String collection,
                                           String storageURL,
                                           AuthHeaderProvider authHeaderProvider,
                                           InfoCollections infoCollections,
                                           InfoConfiguration infoConfiguration,
                                           long limit,
                                           String sort,
                                           JSONRecordFetcher countFetcher)
    throws URISyntaxException {
    super(collection, storageURL, authHeaderProvider, infoCollections, infoConfiguration, limit, sort);
    if (countFetcher == null) {
      throw new IllegalArgumentException("countFetcher must not be null");
    }
    this.countFetcher = countFetcher;
  }

  @Override
  public void createSession(RepositorySessionCreationDelegate delegate,
                            Context context) {
    delegate.onSessionCreated(new CountCheckingServer11RepositorySession(this, this.getDefaultFetchLimit()));
  }

  public class CountCheckingServer11RepositorySession extends Server11RepositorySession {
    private static final String LOG_TAG = "CountCheckingServer11RepositorySession";

    /**
     * The session will report no data available if this is a first sync
     * and the server has more data available than this limit.
     */
    private final long fetchLimit;

    public CountCheckingServer11RepositorySession(Repository repository, long fetchLimit) {
      super(repository);
      this.fetchLimit = fetchLimit;
    }

    @Override
    public boolean shouldSkip() {
      // If this is a first sync, verify that we aren't going to blow through our limit.
      final long lastSyncTimestamp = getLastSyncTimestamp();
      if (lastSyncTimestamp > 0) {
        Logger.info(LOG_TAG, "Collection " + collection + " has already had a first sync: " +
            "timestamp is " + lastSyncTimestamp  + "; " +
            "ignoring any updated counts and syncing as usual.");
      } else {
        Logger.info(LOG_TAG, "Collection " + collection + " is starting a first sync; checking counts.");

        final InfoCounts counts;
        try {
          // This'll probably be the same object, but best to obey the API.
          counts = new InfoCounts(countFetcher.fetchBlocking());
        } catch (Exception e) {
          Logger.warn(LOG_TAG, "Skipping " + collection + " until we can fetch counts.", e);
          return true;
        }

        Integer c = counts.getCount(collection);
        if (c == null) {
          Logger.info(LOG_TAG, "Fetched counts does not include collection " + collection + "; syncing as usual.");
          return false;
        }

        Logger.info(LOG_TAG, "First sync for " + collection + ": " + c + " items.");
        if (c > fetchLimit) {
          Logger.warn(LOG_TAG, "Too many items to sync safely. Skipping.");
          return true;
        }
      }
      return super.shouldSkip();
    }
  }
}
