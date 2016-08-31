/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.delegates;

import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.net.SyncStorageResponse;

/**
 * A fairly generic delegate to handle fetches of single JSON object blobs, as
 * provided by <code>info/configuration</code>, <code>info/collections</code>
 * and <code>info/collection_counts</code>.
 */
public interface JSONRecordFetchDelegate {
  public void handleSuccess(ExtendedJSONObject body);
  public void handleFailure(SyncStorageResponse response);
  public void handleError(Exception e);
}
