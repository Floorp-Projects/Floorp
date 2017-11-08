/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.android.sync.test.helpers;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.testhelpers.WaitHelper;

public class ExpectSuccessDelegate {
  public WaitHelper waitHelper;

  public ExpectSuccessDelegate(WaitHelper waitHelper) {
    this.waitHelper = waitHelper;
  }

  public void performNotify() {
    this.waitHelper.performNotify();
  }

  public void performNotify(Throwable e) {
    this.waitHelper.performNotify(e);
  }

  public String logTag() {
    return this.getClass().getSimpleName();
  }

  public void log(String message) {
    Logger.info(logTag(), message);
  }

  public void log(String message, Throwable throwable) {
    Logger.warn(logTag(), message, throwable);
  }
}