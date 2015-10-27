/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.common;

import java.text.SimpleDateFormat;
import java.util.Locale;
import java.util.TimeZone;

import junit.framework.TestCase;

import org.mozilla.gecko.background.common.DateUtils.DateFormatter;
//import android.util.SparseArray;

public class TestDateUtils extends TestCase {
  // Our old, correct implementation -- used to test the new one.
  public static String getDateStringUsingFormatter(long time) {
    final SimpleDateFormat format = new SimpleDateFormat("yyyy-MM-dd", Locale.US);
    format.setTimeZone(TimeZone.getTimeZone("UTC"));
    return format.format(time);
  }

  private void checkDateString(long time) {
    assertEquals(getDateStringUsingFormatter(time),
                 new DateUtils.DateFormatter().getDateString(time));
  }

  public void testDateImplementations() {
    checkDateString(1L);
    checkDateString(System.currentTimeMillis());
    checkDateString(1379118065844L);
    checkDateString(1379110000000L);
    for (long i = 0L; i < (2 * GlobalConstants.MILLISECONDS_PER_DAY); i += 11000) {
      checkDateString(i);
    }
  }

  @SuppressWarnings("static-method")
  public void testReuse() {
    DateFormatter formatter = new DateFormatter();
    long time = System.currentTimeMillis();
    assertEquals(formatter.getDateString(time), formatter.getDateString(time));
  }

  // Perf tests. Disabled until you need them.
  /*
  @SuppressWarnings("static-method")
  public void testDateTiming() {
    long start = 1379118000000L;
    long end   = 1379118045844L;

    long t0 = android.os.SystemClock.elapsedRealtime();
    for (long i = start; i < end; ++i) {
      DateUtils.getDateString(i);
    }
    long t1 = android.os.SystemClock.elapsedRealtime();
    System.err.println("CALENDAR: " + (t1 - t0));


    t0 = android.os.SystemClock.elapsedRealtime();
    for (long i = start; i < end; ++i) {
      getDateStringFormatter(i);
    }
    t1 = android.os.SystemClock.elapsedRealtime();
    System.err.println("FORMATTER: " + (t1 - t0));
  }

  @SuppressWarnings("static-method")
  public void testDayTiming() {
    long start = 33 * 365;
    long end   = start + 90;
    int reps   = 1;
    long t0 = android.os.SystemClock.elapsedRealtime();
    for (long i = start; i < end; ++i) {
      for (int j = 0; j < reps; ++j) {
        DateUtils.getDateStringForDay(i);
      }
    }
    long t1 = android.os.SystemClock.elapsedRealtime();
    System.err.println("Non-memo: " + (t1 - t0));
  }
  */
}
