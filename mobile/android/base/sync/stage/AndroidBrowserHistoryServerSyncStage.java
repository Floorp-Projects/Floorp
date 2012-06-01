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
import org.mozilla.gecko.sync.repositories.android.AndroidBrowserHistoryRepository;
import org.mozilla.gecko.sync.repositories.android.FennecControlHelper;
import org.mozilla.gecko.sync.repositories.domain.HistoryRecordFactory;
import org.mozilla.gecko.sync.repositories.domain.VersionConstants;

public class AndroidBrowserHistoryServerSyncStage extends ServerSyncStage {
  protected static final String LOG_TAG = "HistoryStage";

  // Eventually this kind of sync stage will be data-driven,
  // and all this hard-coding can go away.
  private static final String HISTORY_SORT          = "index";
  private static final long   HISTORY_REQUEST_LIMIT = 250;

  public AndroidBrowserHistoryServerSyncStage(GlobalSession session) {
    super(session);
  }

  @Override
  protected String getCollection() {
    return "history";
  }

  @Override
  protected String getEngineName() {
    return "history";
  }

  @Override
  public Integer getStorageVersion() {
    return VersionConstants.HISTORY_ENGINE_VERSION;
  }

  @Override
  protected Repository getLocalRepository() {
    return new AndroidBrowserHistoryRepository();
  }

  @Override
  protected Repository getRemoteRepository() throws URISyntaxException {
    return new ConstrainedServer11Repository(session.config.getClusterURLString(),
                                             session.config.username,
                                             getCollection(),
                                             session,
                                             HISTORY_REQUEST_LIMIT,
                                             HISTORY_SORT);
  }

  @Override
  protected RecordFactory getRecordFactory() {
    return new HistoryRecordFactory();
  }

  @Override
  protected boolean isEnabled() throws MetaGlobalException {
    if (session == null || session.getContext() == null) {
      return false;
    }
    boolean migrated = FennecControlHelper.isHistoryMigrated(session.getContext());
    if (!migrated) {
      Logger.warn(LOG_TAG, "Not enabling history engine since Fennec history is not migrated.");
    }
    return super.isEnabled() && migrated;
  }
}
