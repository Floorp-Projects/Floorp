/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.healthreport;

import java.text.SimpleDateFormat;
import java.util.Locale;
import java.util.TimeZone;

import org.mozilla.apache.commons.codec.digest.DigestUtils;

import android.content.ContentUris;
import android.net.Uri;

public class HealthReportUtils {
  public static int getDay(final long time) {
    return (int) Math.floor(time / HealthReportConstants.MILLISECONDS_PER_DAY);
  }

  public static String getEnvironmentHash(final String input) {
    return DigestUtils.shaHex(input);
  }

  public static String getDateStringForDay(long day) {
    return getDateString(HealthReportConstants.MILLISECONDS_PER_DAY * day);
  }

  public static String getDateString(long time) {
    final SimpleDateFormat format = new SimpleDateFormat("yyyy-MM-dd", Locale.US);
    format.setTimeZone(TimeZone.getTimeZone("UTC"));
    return format.format(time);
  }

  /**
   * Take an environment URI (one that identifies an environment) and produce an
   * event URI.
   *
   * That this is needed is tragic.
   *
   * @param environmentURI
   *          the {@link Uri} returned by an environment operation.
   * @return a {@link Uri} to which insertions can be dispatched.
   */
  public static Uri getEventURI(Uri environmentURI) {
    return environmentURI.buildUpon().path("/events/" + ContentUris.parseId(environmentURI) + "/").build();
  }
}
