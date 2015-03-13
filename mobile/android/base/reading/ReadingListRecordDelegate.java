/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.reading;

import org.mozilla.gecko.sync.net.MozResponse;

/**
 * Delegate for downloading records.
 *
 * onRecordReceived will be called at most once per record.
 * onComplete will be called at the end of a successful download.
 *
 * Otherwise, one of the failure methods will be called.
 *
 * onRecordMissingOrDeleted will only be called when fetching a single
 * record by ID.
 */
public interface ReadingListRecordDelegate {
  void onRecordReceived(ServerReadingListRecord record);
  void onComplete(ReadingListResponse response);
  void onFailure(MozResponse response);
  void onFailure(Exception error);
  void onRecordMissingOrDeleted(String guid, ReadingListResponse resp);
}
