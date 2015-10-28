/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.testhelpers;

import java.net.URI;

import org.mozilla.gecko.sync.GlobalSession;
import org.mozilla.gecko.sync.delegates.GlobalSessionCallback;
import org.mozilla.gecko.sync.stage.GlobalSyncStage.Stage;

public class DefaultGlobalSessionCallback implements GlobalSessionCallback {

  @Override
  public void requestBackoff(long backoff) {
  }

  @Override
  public boolean wantNodeAssignment() {
    return false;
  }

  @Override
  public void informUnauthorizedResponse(GlobalSession globalSession,
                                         URI oldClusterURL) {
  }

  @Override
  public void informNodeAssigned(GlobalSession globalSession,
                                 URI oldClusterURL, URI newClusterURL) {
  }

  @Override
  public void informNodeAuthenticationFailed(GlobalSession globalSession,
                                             URI failedClusterURL) {
  }

  @Override
  public void informUpgradeRequiredResponse(GlobalSession session) {
  }

  @Override
  public void informMigrated(GlobalSession globalSession) {
  }

  @Override
  public void handleAborted(GlobalSession globalSession, String reason) {
  }

  @Override
  public void handleError(GlobalSession globalSession, Exception ex) {
  }

  @Override
  public void handleSuccess(GlobalSession globalSession) {
  }

  @Override
  public void handleStageCompleted(Stage currentState,
                                   GlobalSession globalSession) {
  }

  @Override
  public boolean shouldBackOffStorage() {
    return false;
  }

  @Override
  public String nodeWeaveURL() {
    return null;
  }
}
