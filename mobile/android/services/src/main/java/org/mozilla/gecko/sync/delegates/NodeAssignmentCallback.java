/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.delegates;

import java.net.URI;

import org.mozilla.gecko.sync.GlobalSession;

public interface NodeAssignmentCallback {
  /**
   * If true, request node assignment from the server, i.e., fetch node/weave cluster URL.
   */
  public boolean wantNodeAssignment();

  /**
   * Called when a new node is assigned. If there already was an old node, the
   * new node is different from the old node assignment, indicating node
   * reassignment. If there wasn't an old node, we've been freshly assigned.
   *
   * @param globalSession
   * @param oldClusterURL
   *          The old node/weave cluster URL (possibly null).
   * @param newClusterURL
   *          The new node/weave cluster URL (not null).
   */
  public void informNodeAssigned(GlobalSession globalSession, URI oldClusterURL, URI newClusterURL);

  /**
   * Called when wantNodeAssignment() is true, and the new node assignment is
   * the same as the old node assignment, indicating a user authentication
   * error.
   *
   * @param globalSession
   * @param failedClusterURL
   *          The new node/weave cluster URL.
   */
  public void informNodeAuthenticationFailed(GlobalSession globalSession, URI failedClusterURL);

  public String nodeWeaveURL();
}
