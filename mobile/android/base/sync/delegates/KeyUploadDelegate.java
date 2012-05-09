/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.delegates;

public interface KeyUploadDelegate {
  /**
   * Called when keys have been successfully uploaded to the server.
   * <p>
   * The uploaded keys are intentionally not exposed. It is possible for two
   * clients to simultaneously upload keys and for each client to conclude that
   * its keys are current (since the server returned 200 on upload). To shorten
   * the window wherein two such clients can race, all clients should upload and
   * then immediately re-download the fetched keys.
   * <p>
   * See Bug 692700, Bug 693893.
   */
  void onKeysUploaded();
  void onKeyUploadFailed(Exception e);
}
