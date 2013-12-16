/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.domain;

import org.mozilla.gecko.sync.CryptoRecord;
import org.mozilla.gecko.sync.repositories.RecordFactory;
import org.mozilla.gecko.sync.repositories.domain.PasswordRecord;
import org.mozilla.gecko.sync.repositories.domain.Record;

public class PasswordRecordFactory extends RecordFactory {
  @Override
  public Record createRecord(Record record) {
    PasswordRecord r = new PasswordRecord();
    r.initFromEnvelope((CryptoRecord) record);
    return r;
  }
}
