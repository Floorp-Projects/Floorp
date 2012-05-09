/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.delegates;

import java.net.URI;

import org.mozilla.gecko.sync.GlobalSession;
import org.mozilla.gecko.sync.stage.GlobalSyncStage.Stage;

public interface GlobalSessionCallback {

  /**
   * Request that no further syncs occur within the next `backoff` milliseconds.
   * @param backoff a duration in milliseconds.
   */
  void requestBackoff(long backoff);

  /**
   * If true, request node assignment from the server, i.e., fetch node/weave cluster URL.
   */
  boolean wantNodeAssignment();

  /**
   * Called on a 401 HTTP response.
   */
  void informUnauthorizedResponse(GlobalSession globalSession, URI oldClusterURL);

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
  void informNodeAssigned(GlobalSession globalSession, URI oldClusterURL, URI newClusterURL);

  /**
   * Called when wantNodeAssignment() is true, and the new node assignment is
   * the same as the old node assignment, indicating a user authentication
   * error.
   *
   * @param globalSession
   * @param failedClusterURL
   *          The new node/weave cluster URL.
   */
  void informNodeAuthenticationFailed(GlobalSession globalSession, URI failedClusterURL);

  /**
   * Called when an HTTP failure indicates that a software upgrade is required.
   */
  void informUpgradeRequiredResponse(GlobalSession session);

  void handleAborted(GlobalSession globalSession, String reason);
  void handleError(GlobalSession globalSession, Exception ex);
  void handleSuccess(GlobalSession globalSession);
  void handleStageCompleted(Stage currentState, GlobalSession globalSession);

  boolean shouldBackOff();

}
