/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.net;

public interface SyncStorageRequestDelegate {
  String credentials();
  String ifUnmodifiedSince();

  // TODO: at this point we can access X-Weave-Timestamp, compare
  // that to our local timestamp, and compute an estimate of clock
  // skew. Bug 721887.
  void handleRequestSuccess(SyncStorageResponse response);
  void handleRequestFailure(SyncStorageResponse response);
  void handleRequestError(Exception ex);
}
