/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.helpers;

import junit.framework.AssertionFailedError;

import org.mozilla.gecko.background.testhelpers.WaitHelper;

import android.app.Activity;
import android.content.Context;
import android.test.ActivityInstrumentationTestCase2;

/**
 * AndroidSyncTestCase provides helper methods for testing.
 */
public class AndroidSyncTestCase extends ActivityInstrumentationTestCase2<Activity> {
  protected static String LOG_TAG = "AndroidSyncTestCase";

  public AndroidSyncTestCase() {
    super(Activity.class);
    WaitHelper.resetTestWaiter();
  }

  public Context getApplicationContext() {
    return this.getInstrumentation().getTargetContext().getApplicationContext();
  }

  public static void performWait(Runnable runnable) {
    try {
      WaitHelper.getTestWaiter().performWait(runnable);
    } catch (WaitHelper.InnerError e) {
      AssertionFailedError inner = new AssertionFailedError("Caught error in performWait");
      inner.initCause(e.innerError);
      throw inner;
    }
  }

  public static void performNotify() {
    WaitHelper.getTestWaiter().performNotify();
  }

  public static void performNotify(Throwable e) {
    WaitHelper.getTestWaiter().performNotify(e);
  }

  public static void performNotify(String reason, Throwable e) {
    AssertionFailedError er = new AssertionFailedError(reason + ": " + e.getMessage());
    er.initCause(e);
    WaitHelper.getTestWaiter().performNotify(er);
  }
}
