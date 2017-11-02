/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.android.sync.test.helpers;

import junit.framework.AssertionFailedError;
import org.mozilla.gecko.background.testhelpers.WaitHelper;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.RepositorySessionBundle;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFinishDelegate;

import java.util.concurrent.ExecutorService;

public class ExpectSuccessRepositorySessionFinishDelegate extends
    ExpectSuccessDelegate implements RepositorySessionFinishDelegate {

  public ExpectSuccessRepositorySessionFinishDelegate(WaitHelper waitHelper) {
    super(waitHelper);
  }

  @Override
  public void onFinishFailed(Exception ex) {
    log("Finish failed.", ex);
    performNotify(new AssertionFailedError("onFinishFailed: finish should not have failed."));
  }

  @Override
  public void onFinishSucceeded(RepositorySession session, RepositorySessionBundle bundle) {
    log("Finish succeeded.");
    performNotify();
  }

  @Override
  public RepositorySessionFinishDelegate deferredFinishDelegate(ExecutorService executor) {
    return this;
  }
}
