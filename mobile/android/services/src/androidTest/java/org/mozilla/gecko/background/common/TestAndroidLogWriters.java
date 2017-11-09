/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.common;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.common.log.writers.AndroidLevelCachingLogWriter;
import org.mozilla.gecko.background.common.log.writers.AndroidLogWriter;
import org.mozilla.gecko.background.common.log.writers.LogWriter;
import org.mozilla.gecko.background.helpers.AndroidSyncTestCase;

public class TestAndroidLogWriters extends AndroidSyncTestCase {
  public static final String TEST_LOG_TAG = "TestAndroidLogWriters";

  public static final String TEST_MESSAGE_1 = "LOG TEST MESSAGE one";
  public static final String TEST_MESSAGE_2 = "LOG TEST MESSAGE two";
  public static final String TEST_MESSAGE_3 = "LOG TEST MESSAGE three";

  public void setUp() {
    Logger.stopLoggingToAll();
  }

  public void tearDown() {
    Logger.resetLogging();
  }

  /**
   * Verify these *all* appear in the Android log by using
   * <code>adb logcat | grep TestAndroidLogWriters</code> after executing
   * <code>adb shell setprop log.tag.TestAndroidLogWriters ERROR</code>.
   * <p>
   * This writer does not use the Android log levels!
   */
  public void testAndroidLogWriter() {
    LogWriter lw = new AndroidLogWriter();

    Logger.error(TEST_LOG_TAG, TEST_MESSAGE_1, new RuntimeException());
    Logger.startLoggingTo(lw);
    Logger.error(TEST_LOG_TAG, TEST_MESSAGE_2);
    Logger.warn(TEST_LOG_TAG, TEST_MESSAGE_2);
    Logger.info(TEST_LOG_TAG, TEST_MESSAGE_2);
    Logger.debug(TEST_LOG_TAG, TEST_MESSAGE_2);
    Logger.trace(TEST_LOG_TAG, TEST_MESSAGE_2);
    Logger.stopLoggingTo(lw);
    Logger.error(TEST_LOG_TAG, TEST_MESSAGE_3, new RuntimeException());
  }

  /**
   * Verify only *some* of these appear in the Android log by using
   * <code>adb logcat | grep TestAndroidLogWriters</code> after executing
   * <code>adb shell setprop log.tag.TestAndroidLogWriters INFO</code>.
   * <p>
   * This writer should use the Android log levels!
   */
  public void testAndroidLevelCachingLogWriter() throws Exception {
    LogWriter lw = new AndroidLevelCachingLogWriter(new AndroidLogWriter());

    Logger.error(TEST_LOG_TAG, TEST_MESSAGE_1, new RuntimeException());
    Logger.startLoggingTo(lw);
    Logger.error(TEST_LOG_TAG, TEST_MESSAGE_2);
    Logger.warn(TEST_LOG_TAG, TEST_MESSAGE_2);
    Logger.info(TEST_LOG_TAG, TEST_MESSAGE_2);
    Logger.debug(TEST_LOG_TAG, TEST_MESSAGE_2);
    Logger.trace(TEST_LOG_TAG, TEST_MESSAGE_2);
    Logger.stopLoggingTo(lw);
    Logger.error(TEST_LOG_TAG, TEST_MESSAGE_3, new RuntimeException());
  }
}
