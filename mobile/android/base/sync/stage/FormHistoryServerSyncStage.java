/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.stage;

import java.net.URISyntaxException;

import org.mozilla.gecko.sync.CryptoRecord;
import org.mozilla.gecko.sync.repositories.ConstrainedServer11Repository;
import org.mozilla.gecko.sync.repositories.RecordFactory;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.android.FormHistoryRepositorySession;
import org.mozilla.gecko.sync.repositories.domain.FormHistoryRecord;
import org.mozilla.gecko.sync.repositories.domain.Record;
import org.mozilla.gecko.sync.repositories.domain.VersionConstants;

public class FormHistoryServerSyncStage extends ServerSyncStage {

  // Eventually this kind of sync stage will be data-driven,
  // and all this hard-coding can go away.
  private static final String FORM_HISTORY_SORT          = "index";
  private static final long   FORM_HISTORY_REQUEST_LIMIT = 5000;         // Sanity limit.

  @Override
  protected String getCollection() {
    return "forms";
  }

  @Override
  protected String getEngineName() {
    return "forms";
  }

  @Override
  public Integer getStorageVersion() {
    return VersionConstants.FORMS_ENGINE_VERSION;
  }

  @Override
  protected Repository getRemoteRepository() throws URISyntaxException {
    String collection = getCollection();
    return new ConstrainedServer11Repository(
                                             collection,
                                             session.config.storageURL(),
                                             session.getAuthHeaderProvider(),
                                             FORM_HISTORY_REQUEST_LIMIT,
                                             FORM_HISTORY_SORT);
  }

  @Override
  protected Repository getLocalRepository() {
    return new FormHistoryRepositorySession.FormHistoryRepository();
  }

  public class FormHistoryRecordFactory extends RecordFactory {

    @Override
    public Record createRecord(Record record) {
      FormHistoryRecord r = new FormHistoryRecord();
      r.initFromEnvelope((CryptoRecord) record);
      return r;
    }
  }

  @Override
  protected RecordFactory getRecordFactory() {
    return new FormHistoryRecordFactory();
  }
}
