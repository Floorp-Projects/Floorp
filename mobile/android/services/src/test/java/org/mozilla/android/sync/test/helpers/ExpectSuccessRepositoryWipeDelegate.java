/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.android.sync.test.helpers;

import junit.framework.AssertionFailedError;
import org.mozilla.gecko.background.testhelpers.WaitHelper;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionWipeDelegate;

import java.util.concurrent.ExecutorService;

public class ExpectSuccessRepositoryWipeDelegate extends ExpectSuccessDelegate
    implements RepositorySessionWipeDelegate {

  public ExpectSuccessRepositoryWipeDelegate(WaitHelper waitHelper) {
    super(waitHelper);
  }

  @Override
  public void onWipeSucceeded() {
    log("Wipe succeeded.");
    performNotify();
  }

  @Override
  public void onWipeFailed(Exception ex) {
    log("Wipe failed.", ex);
    performNotify(new AssertionFailedError("onWipeFailed: wipe should not have failed."));
  }

  @Override
  public RepositorySessionWipeDelegate deferredWipeDelegate(ExecutorService executor) {
    log("Wipe deferred.");
    return this;
  }
}
