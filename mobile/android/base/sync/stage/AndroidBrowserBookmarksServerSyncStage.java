/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.stage;

import java.net.URISyntaxException;

import org.mozilla.gecko.sync.JSONRecordFetcher;
import org.mozilla.gecko.sync.MetaGlobalException;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.repositories.RecordFactory;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.android.AndroidBrowserBookmarksRepository;
import org.mozilla.gecko.sync.repositories.domain.BookmarkRecordFactory;
import org.mozilla.gecko.sync.repositories.domain.VersionConstants;

public class AndroidBrowserBookmarksServerSyncStage extends ServerSyncStage {
  protected static final String LOG_TAG = "BookmarksStage";

  // Eventually this kind of sync stage will be data-driven,
  // and all this hard-coding can go away.
  private static final String BOOKMARKS_SORT          = "index";
  private static final long   BOOKMARKS_REQUEST_LIMIT = 5000;         // Sanity limit.

  @Override
  protected String getCollection() {
    return "bookmarks";
  }

  @Override
  protected String getEngineName() {
    return "bookmarks";
  }

  @Override
  public Integer getStorageVersion() {
    return VersionConstants.BOOKMARKS_ENGINE_VERSION;
  }

  @Override
  protected Repository getRemoteRepository() throws URISyntaxException {
    // If this is a first sync, we need to check server counts to make sure that we aren't
    // going to screw up. SafeConstrainedServer11Repository does this. See Bug 814331.
    AuthHeaderProvider authHeaderProvider = session.getAuthHeaderProvider();
    final JSONRecordFetcher countsFetcher = new JSONRecordFetcher(session.config.infoCollectionCountsURL(), authHeaderProvider);
    String collection = getCollection();
    return new SafeConstrainedServer11Repository(
        collection,
        session.config.storageURL(),
        session.getAuthHeaderProvider(),
        session.config.infoCollections,
        BOOKMARKS_REQUEST_LIMIT,
        BOOKMARKS_SORT,
        countsFetcher);
  }

  @Override
  protected Repository getLocalRepository() {
    return new AndroidBrowserBookmarksRepository();
  }

  @Override
  protected RecordFactory getRecordFactory() {
    return new BookmarkRecordFactory();
  }

  @Override
  protected boolean isEnabled() throws MetaGlobalException {
    if (session == null || session.getContext() == null) {
      return false;
    }
    return super.isEnabled();
  }
}
