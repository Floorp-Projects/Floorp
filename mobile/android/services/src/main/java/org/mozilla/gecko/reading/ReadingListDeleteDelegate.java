/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.reading;

import org.mozilla.gecko.sync.net.MozResponse;

/**
 * Response delegate for a server DELETE.
 * Only one of these methods will be called, and it will be called precisely once,
 * unless batching is used.
 */
public interface ReadingListDeleteDelegate {
  void onSuccess(ReadingListRecordResponse response, ReadingListRecord record);
  void onPreconditionFailed(String guid, MozResponse response);
  void onRecordMissingOrDeleted(String guid, MozResponse response);
  void onFailure(Exception e);
  void onFailure(MozResponse response);
  void onBatchDone();
}
