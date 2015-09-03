/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.android.sync.test.helpers;

import junit.framework.AssertionFailedError;

import org.mozilla.gecko.background.testhelpers.WaitHelper;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionCreationDelegate;

public class ExpectSuccessRepositorySessionCreationDelegate extends
    ExpectSuccessDelegate implements RepositorySessionCreationDelegate {

  public ExpectSuccessRepositorySessionCreationDelegate(WaitHelper waitHelper) {
    super(waitHelper);
  }

  @Override
  public void onSessionCreateFailed(Exception ex) {
    log("Session creation failed.", ex);
    performNotify(new AssertionFailedError("onSessionCreateFailed: session creation should not have failed."));
  }

  @Override
  public void onSessionCreated(RepositorySession session) {
    log("Session creation succeeded.");
    performNotify();
  }

  @Override
  public RepositorySessionCreationDelegate deferredCreationDelegate() {
    log("Session creation deferred.");
    return this;
  }

}
