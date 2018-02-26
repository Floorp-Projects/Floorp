/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.stage;

import org.mozilla.gecko.sync.repositories.RecordFactory;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.android.PasswordsRepositorySession;
import org.mozilla.gecko.sync.repositories.domain.PasswordRecordFactory;
import org.mozilla.gecko.sync.repositories.domain.VersionConstants;

public class PasswordsServerSyncStage extends ServerSyncStage {
  @Override
  protected String getCollection() {
    return "passwords";
  }

  @Override
  protected String getEngineName() {
    return "passwords";
  }

  @Override
  public Integer getStorageVersion() {
    return VersionConstants.PASSWORDS_ENGINE_VERSION;
  }

  @Override
  protected Repository getLocalRepository() {
    return new PasswordsRepositorySession.PasswordsRepository();
  }

  @Override
  protected RecordFactory getRecordFactory() {
    return new PasswordRecordFactory();
  }
}
