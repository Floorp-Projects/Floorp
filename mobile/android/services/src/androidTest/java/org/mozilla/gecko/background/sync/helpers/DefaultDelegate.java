/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.sync.helpers;

import java.util.concurrent.ExecutorService;

import junit.framework.AssertionFailedError;

import org.mozilla.gecko.background.testhelpers.WaitHelper;

public abstract class DefaultDelegate {
  protected ExecutorService executor;

  protected final WaitHelper waitHelper;

  public DefaultDelegate() {
    waitHelper = WaitHelper.getTestWaiter();
  }

  public DefaultDelegate(WaitHelper waitHelper) {
    this.waitHelper = waitHelper;
  }

  protected WaitHelper getTestWaiter() {
    return waitHelper;
  }

  public void performWait(Runnable runnable) throws AssertionFailedError {
    getTestWaiter().performWait(runnable);
  }

  public void performNotify() {
    getTestWaiter().performNotify();
  }

  public void performNotify(Throwable e) {
    getTestWaiter().performNotify(e);
  }

  public void performNotify(String reason, Throwable e) {
    String message = reason;
    if (e != null) {
      message += ": " + e.getMessage();
    }
    AssertionFailedError ex = new AssertionFailedError(message);
    if (e != null) {
      ex.initCause(e);
    }
    getTestWaiter().performNotify(ex);
  }
}
