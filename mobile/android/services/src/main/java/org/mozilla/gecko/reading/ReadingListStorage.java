/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.reading;

import android.database.Cursor;

public interface ReadingListStorage {
  Cursor getModified();
  Cursor getDeletedItems();
  Cursor getStatusChanges();
  Cursor getNew();
  Cursor getAll();
  ReadingListChangeAccumulator getChangeAccumulator();
}
