/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.common.log.writers.test;

import android.util.Log;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.common.log.writers.LevelFilteringLogWriter;
import org.mozilla.gecko.background.common.log.writers.LogWriter;
import org.mozilla.gecko.background.common.log.writers.PrintLogWriter;
import org.mozilla.gecko.background.common.log.writers.SimpleTagLogWriter;
import org.mozilla.gecko.background.common.log.writers.StringLogWriter;
import org.mozilla.gecko.background.common.log.writers.ThreadLocalTagLogWriter;
import org.mozilla.gecko.background.testhelpers.TestRunner;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.CountDownLatch;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

@RunWith(TestRunner.class)
public class TestLogWriters {

  public static final String TEST_LOG_TAG_1 = "TestLogTag1";
  public static final String TEST_LOG_TAG_2 = "TestLogTag2";

  public static final String TEST_MESSAGE_1  = "LOG TEST MESSAGE one";
  public static final String TEST_MESSAGE_2  = "LOG TEST MESSAGE two";
  public static final String TEST_MESSAGE_3  = "LOG TEST MESSAGE three";

  @Before
  public void setUp() {
    Logger.stopLoggingToAll();
  }

  @After
  public void tearDown() {
    Logger.stopLoggingToAll();
  }

  @Test
  public void testStringLogWriter() {
    StringLogWriter lw = new StringLogWriter();

    Logger.error(TEST_LOG_TAG_1, TEST_MESSAGE_1, new RuntimeException());
    Logger.startLoggingTo(lw);
    Logger.error(TEST_LOG_TAG_1, TEST_MESSAGE_2);
    Logger.warn(TEST_LOG_TAG_1, TEST_MESSAGE_2);
    Logger.info(TEST_LOG_TAG_1, TEST_MESSAGE_2);
    Logger.debug(TEST_LOG_TAG_1, TEST_MESSAGE_2);
    Logger.trace(TEST_LOG_TAG_1, TEST_MESSAGE_2);
    Logger.stopLoggingTo(lw);
    Logger.error(TEST_LOG_TAG_2, TEST_MESSAGE_3, new RuntimeException());

    String s = lw.toString();
    assertFalse(s.contains("RuntimeException"));
    assertFalse(s.contains(".java"));
    assertTrue(s.contains(TEST_LOG_TAG_1));
    assertFalse(s.contains(TEST_LOG_TAG_2));
    assertFalse(s.contains(TEST_MESSAGE_1));
    assertTrue(s.contains(TEST_MESSAGE_2));
    assertFalse(s.contains(TEST_MESSAGE_3));
  }

  @Test
  public void testSingleTagLogWriter() {
    final String SINGLE_TAG = "XXX";
    StringLogWriter lw = new StringLogWriter();

    Logger.startLoggingTo(new SimpleTagLogWriter(SINGLE_TAG, lw));
    Logger.error(TEST_LOG_TAG_1, TEST_MESSAGE_1);
    Logger.warn(TEST_LOG_TAG_2, TEST_MESSAGE_2);

    String s = lw.toString();
    for (String line : s.split("\n")) {
      assertTrue(line.startsWith(SINGLE_TAG));
    }
    assertTrue(s.startsWith(SINGLE_TAG + " :: E :: " + TEST_LOG_TAG_1));
  }

  @Test
  public void testLevelFilteringLogWriter() {
    StringLogWriter lw = new StringLogWriter();

    assertFalse(new LevelFilteringLogWriter(Log.WARN, lw).shouldLogVerbose(TEST_LOG_TAG_1));
    assertTrue(new LevelFilteringLogWriter(Log.VERBOSE, lw).shouldLogVerbose(TEST_LOG_TAG_1));

    Logger.startLoggingTo(new LevelFilteringLogWriter(Log.WARN, lw));
    Logger.error(TEST_LOG_TAG_1, TEST_MESSAGE_2);
    Logger.warn(TEST_LOG_TAG_1, TEST_MESSAGE_2);
    Logger.info(TEST_LOG_TAG_1, TEST_MESSAGE_2);
    Logger.debug(TEST_LOG_TAG_1, TEST_MESSAGE_2);
    Logger.trace(TEST_LOG_TAG_1, TEST_MESSAGE_2);

    String s = lw.toString();
    assertTrue(s.contains(PrintLogWriter.ERROR));
    assertTrue(s.contains(PrintLogWriter.WARN));
    assertFalse(s.contains(PrintLogWriter.INFO));
    assertFalse(s.contains(PrintLogWriter.DEBUG));
    assertFalse(s.contains(PrintLogWriter.VERBOSE));
  }

  @Test
  public void testThreadLocalLogWriter() throws InterruptedException {
    final InheritableThreadLocal<String> logTag = new InheritableThreadLocal<String>() {
      @Override
      protected String initialValue() {
        return "PARENT";
      }
    };

    final StringLogWriter stringLogWriter = new StringLogWriter();
    final LogWriter logWriter = new ThreadLocalTagLogWriter(logTag, stringLogWriter);

    try {
      Logger.startLoggingTo(logWriter);

      Logger.info("parent tag before", "parent message before");

      int threads = 3;
      final CountDownLatch latch = new CountDownLatch(threads);

      for (int thread = 0; thread < threads; thread++) {
        final int threadNumber = thread;

        new Thread(new Runnable() {
          @Override
          public void run() {
            try {
              logTag.set("CHILD" + threadNumber);
              Logger.info("child tag " + threadNumber, "child message " + threadNumber);
            } finally {
              latch.countDown();
            }
          }
        }).start();
      }

      latch.await();

      Logger.info("parent tag after", "parent message after");

      String s = stringLogWriter.toString();
      List<String> lines = Arrays.asList(s.split("\n"));

      // Because tests are run in a multi-threaded environment, we get
      // additional logs that are not generated by this test. So we test that we
      // get all the messages in a reasonable order.
      try {
        int parent1 = lines.indexOf("PARENT :: I :: parent tag before :: parent message before");
        int parent2 = lines.indexOf("PARENT :: I :: parent tag after :: parent message after");

        assertTrue(parent1 >= 0);
        assertTrue(parent2 >= 0);
        assertTrue(parent1 < parent2);

        for (int thread = 0; thread < threads; thread++) {
          int child = lines.indexOf("CHILD" + thread + " :: I :: child tag " + thread + " :: child message " + thread);
          assertTrue(child >= 0);
          assertTrue(parent1 < child);
          assertTrue(child < parent2);
        }
      } catch (Throwable e) {
        // Shouldn't happen.  Let's dump to aid debugging.
        e.printStackTrace();
        assertEquals("\0", s);
      }
    } finally {
      Logger.stopLoggingTo(logWriter);
    }
  }
}