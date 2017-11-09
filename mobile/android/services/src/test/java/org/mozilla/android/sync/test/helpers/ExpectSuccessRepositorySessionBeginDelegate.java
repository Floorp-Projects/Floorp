/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.android.sync.test.helpers;

import junit.framework.AssertionFailedError;
import org.mozilla.gecko.background.testhelpers.WaitHelper;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionBeginDelegate;

import java.util.concurrent.ExecutorService;

public class ExpectSuccessRepositorySessionBeginDelegate
extends ExpectSuccessDelegate
implements RepositorySessionBeginDelegate {

  public ExpectSuccessRepositorySessionBeginDelegate(WaitHelper waitHelper) {
    super(waitHelper);
  }

  @Override
  public void onBeginFailed(Exception ex) {
    log("Session begin failed.", ex);
    performNotify(new AssertionFailedError("Session begin failed: " + ex.getMessage()));
  }

  @Override
  public void onBeginSucceeded(RepositorySession session) {
    log("Session begin succeeded.");
    performNotify();
  }

  @Override
  public RepositorySessionBeginDelegate deferredBeginDelegate(ExecutorService executor) {
    log("Session begin delegate deferred.");
    return this;
  }
}
