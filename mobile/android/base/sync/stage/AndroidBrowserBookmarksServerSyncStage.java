/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.stage;

import java.net.URISyntaxException;

import org.mozilla.gecko.sync.GlobalSession;
import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.MetaGlobalException;
import org.mozilla.gecko.sync.repositories.ConstrainedServer11Repository;
import org.mozilla.gecko.sync.repositories.RecordFactory;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.android.AndroidBrowserBookmarksRepository;
import org.mozilla.gecko.sync.repositories.android.FennecControlHelper;
import org.mozilla.gecko.sync.repositories.domain.BookmarkRecordFactory;
import org.mozilla.gecko.sync.repositories.domain.VersionConstants;

public class AndroidBrowserBookmarksServerSyncStage extends ServerSyncStage {
  protected static final String LOG_TAG = "BookmarksStage";

  // Eventually this kind of sync stage will be data-driven,
  // and all this hard-coding can go away.
  private static final String BOOKMARKS_SORT          = "index";
  private static final long   BOOKMARKS_REQUEST_LIMIT = 5000;         // Sanity limit.

  public AndroidBrowserBookmarksServerSyncStage(GlobalSession session) {
    super(session);
  }

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
    return new ConstrainedServer11Repository(session.config.getClusterURLString(),
                                             session.config.username,
                                             getCollection(),
                                             session,
                                             BOOKMARKS_REQUEST_LIMIT,
                                             BOOKMARKS_SORT);
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
    if (session.getContext() == null) {
      return false;
    }
    boolean migrated = FennecControlHelper.areBookmarksMigrated(session.getContext());
    if (!migrated) {
      Logger.warn(LOG_TAG, "Not enabling bookmarks engine since Fennec bookmarks are not migrated.");
    }
    return super.isEnabled() && migrated;
  }
}