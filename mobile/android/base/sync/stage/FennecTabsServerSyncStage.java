/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.stage;

import org.mozilla.gecko.sync.CryptoRecord;
import org.mozilla.gecko.sync.GlobalSession;
import org.mozilla.gecko.sync.repositories.RecordFactory;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.android.FennecTabsRepository;
import org.mozilla.gecko.sync.repositories.domain.Record;
import org.mozilla.gecko.sync.repositories.domain.TabsRecord;
import org.mozilla.gecko.sync.repositories.domain.VersionConstants;

public class FennecTabsServerSyncStage extends ServerSyncStage {
  private static final String COLLECTION = "tabs";

  public FennecTabsServerSyncStage(GlobalSession session) {
    super(session);
  }

  public class FennecTabsRecordFactory extends RecordFactory {
    @Override
    public Record createRecord(Record record) {
      TabsRecord r = new TabsRecord();
      r.initFromEnvelope((CryptoRecord) record);
      return r;
    }
  }

  @Override
  protected String getCollection() {
    return COLLECTION;
  }

  @Override
  protected String getEngineName() {
    return COLLECTION;
  }

  @Override
  public Integer getStorageVersion() {
    return VersionConstants.TABS_ENGINE_VERSION;
  }

  @Override
  protected Repository getLocalRepository() {
    return new FennecTabsRepository();
  }

  @Override
  protected RecordFactory getRecordFactory() {
    return new FennecTabsRecordFactory();
  }
}
