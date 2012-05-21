/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories;

import org.mozilla.gecko.sync.repositories.domain.Record;

// Take a record retrieved from some middleware, producing
// some concrete record type for application to some local repository.
public abstract class RecordFactory {
  public abstract Record createRecord(Record record);
}
