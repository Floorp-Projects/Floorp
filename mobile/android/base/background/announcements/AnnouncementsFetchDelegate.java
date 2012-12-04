/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.announcements;

import java.util.List;
import java.util.Locale;

public interface AnnouncementsFetchDelegate {
  /**
   * @return the timestamp of the last fetch in milliseconds.
   */
  public long getLastFetch();

  /**
   * @return the Date header string of the last response, or null if not present.
   */
  public String getLastDate();

  /**
   * @return the current system locale (e.g., en_us).
   */
  public Locale getLocale();

  /**
   * @return the User-Agent header to use for the request.
   */
  public String getUserAgent();

  /**
   * @return the server URL to interrogate, including path.
   */
  public String getServiceURL();

  /*
   * Callback methods.
   * Note that we provide both a local fetch time and a server date here.
   * This is so we can track how long we've waited (local), and supply the
   * date back to the server for If-Modified-Since.
   */
  public void onNoNewAnnouncements(long localFetchTime, String serverDate);
  public void onNewAnnouncements(List<Announcement> snippets, long localFetchTime, String serverDate);
  public void onLocalError(Exception e);
  public void onRemoteError(Exception e);
  public void onRemoteFailure(int status);
  public void onBackoff(int retryAfterInSeconds);
}