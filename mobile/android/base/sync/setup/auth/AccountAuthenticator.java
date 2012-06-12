/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.setup.auth;

import java.util.LinkedList;
import java.util.Queue;

import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.ThreadPool;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.setup.activities.AccountActivity;

public class AccountAuthenticator {
  private final String LOG_TAG = "AccountAuthenticator";

  private AccountActivity activityCallback;
  private Queue<AuthenticatorStage> stages;

  // Values for authentication.
  public String password;
  public String username;

  public String authServer;
  public String nodeServer;

  public boolean isSuccess = false;
  public boolean isCanceled = false;

  public AccountAuthenticator(AccountActivity activity) {
    activityCallback = activity;
    prepareStages();
  }

  private void prepareStages() {
    stages = new LinkedList<AuthenticatorStage>();
    stages.add(new EnsureUserExistenceStage());
    stages.add(new FetchUserNodeStage());
    stages.add(new AuthenticateAccountStage());
  }

  public void authenticate(String server, String account, String password) {
    // Set authentication values.
    if (!server.endsWith("/")) {
      server += "/";
    }
    nodeServer = server;
    this.password = password;

    // Calculate and save username hash.
    try {
      username = Utils.usernameFromAccount(account);
    } catch (Exception e) {
      abort(AuthenticationResult.FAILURE_OTHER, e);
      return;
    }
    Logger.debug(LOG_TAG, "username:" + username);

    Logger.debug(LOG_TAG, "running first stage.");
    // Start first stage of authentication.
    runNextStage();
  }

  /**
   * Run next stage of authentication.
   */
  public void runNextStage() {
    if (isCanceled) {
      return;
    }
    if (stages.size() == 0) {
      Logger.debug(LOG_TAG, "Authentication completed.");
      activityCallback.authCallback(isSuccess ? AuthenticationResult.SUCCESS : AuthenticationResult.FAILURE_PASSWORD);
      return;
    }
    AuthenticatorStage nextStage = stages.remove();
    try {
      nextStage.execute(this);
    } catch (Exception e) {
      Logger.warn(LOG_TAG, "Unhandled exception in stage " + nextStage);
      abort(AuthenticationResult.FAILURE_OTHER, e);
    }
  }

  /**
   * Abort authentication.
   *
   * @param e
   *    Exception causing abort.
   * @param reason
   *    Reason for abort.
   */
  public void abort(AuthenticationResult result, Exception e) {
    if (isCanceled) {
      return;
    }
    Logger.warn(LOG_TAG, "Authentication failed.", e);
    activityCallback.authCallback(result);
  }

  /* Helper functions */
  public static void runOnThread(Runnable run) {
    ThreadPool.run(run);
  }
}
