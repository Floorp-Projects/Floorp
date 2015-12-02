/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.domain;

import org.mozilla.gecko.sync.CryptoRecord;
import org.mozilla.gecko.sync.repositories.RecordFactory;

/**
 * Turns CryptoRecords into BookmarkRecords.
 *
 * @author rnewman
 *
 */
public class BookmarkRecordFactory extends RecordFactory {

  @Override
  public Record createRecord(Record record) {
    BookmarkRecord r = new BookmarkRecord();
    r.initFromEnvelope((CryptoRecord) record);
    return r;
  }

}
