/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.announcements;

import java.io.UnsupportedEncodingException;
import java.net.URI;
import java.net.URISyntaxException;
import java.net.URLEncoder;

import org.mozilla.gecko.background.common.GlobalConstants;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.net.BaseResource;

public class AnnouncementsFetcher {
  private static final String LOG_TAG = "AnnounceFetch";
  private static final long MILLISECONDS_PER_DAY = 24 * 60 * 60 * 1000;

  public static URI getSnippetURI(String base, String channel,
                                  String version, String platform,
                                  int idleDays)
    throws URISyntaxException {
    try {
      final String c = URLEncoder.encode(channel, "UTF-8");
      final String v = URLEncoder.encode(version, "UTF-8");
      final String p = URLEncoder.encode(platform, "UTF-8");
      final String s = base + c + "/" + v + "/" + p + ((idleDays == -1) ? "" : ("?idle=" + idleDays));
      return new URI(s);
    } catch (UnsupportedEncodingException e) {
      // Nonsense.
      return null;
    }
  }

  public static URI getAnnounceURI(final String baseURL, final long lastLaunch) throws URISyntaxException {
    final String channel = getChannel();
    final String version = getVersion();
    final String platform = getPlatform();
    final int idleDays = getIdleDays(lastLaunch);

    Logger.debug(LOG_TAG, "Fetch URI: idle for " + idleDays + " days.");
    return getSnippetURI(baseURL, channel, version, platform, idleDays);
  }

  protected static String getChannel() {
    return AnnouncementsConstants.ANNOUNCE_CHANNEL;
  }

  protected static String getVersion() {
    return GlobalConstants.MOZ_APP_VERSION;
  }

  protected static String getPlatform() {
    return GlobalConstants.ANDROID_CPU_ARCH;
  }

  /**
   * Return the number of days that we've been idle, assuming that we have a
   * sane last launch time and the current time is within range. If no sane idle
   * time can be returned, we return -1.
   *
   * @param lastLaunch
   *          Time at which the browser was last launched, in milliseconds since epoch.
   * @param now
   *          Milliseconds since epoch for which idle time should be calculated.
   * @return number of idle days, or -1 if out of range.
   */
  protected static int getIdleDays(final long lastLaunch, final long now) {
    if (lastLaunch <= 0) {
      return -1;
    }

    if (now < GlobalConstants.BUILD_TIMESTAMP_MSEC) {
      Logger.warn(LOG_TAG, "Current time " + now + " earlier than build date. Not calculating idle.");
      return -1;
    }

    if (now < lastLaunch) {
      Logger.warn(LOG_TAG, "Current time " + now + " earlier than last launch! Not calculating idle.");
      return -1;
    }

    final long idleMillis = now - lastLaunch;
    final int idleDays = (int) (idleMillis / MILLISECONDS_PER_DAY);

    if (idleDays > AnnouncementsConstants.MAX_SANE_IDLE_DAYS) {
      Logger.warn(LOG_TAG, "Idle from " + lastLaunch + " until " + now +
                           ", which is insane. Not calculating idle.");
      return -1;
    }

    return idleDays;
  }

  /**
   * Return the number of days that we've been idle, assuming that we have a
   * sane last launch time and the current time is within range. If no sane idle
   * time can be returned, we return -1.
   * The current time will be calculated from {@link System#currentTimeMillis()}.
   *
   * @param lastLaunch
   *          Unix timestamp at which the browser was last launched.
   * @return number of idle days, or -1 if out of range.
   */
  protected static int getIdleDays(final long lastLaunch) {
    final long now = System.currentTimeMillis();
    return getIdleDays(lastLaunch, now);
  }

  public static void fetchAnnouncements(URI uri, AnnouncementsFetchDelegate delegate) {
    BaseResource r = new BaseResource(uri);
    r.delegate = new AnnouncementsFetchResourceDelegate(r, delegate);
    r.getBlocking();
  }

  /**
   * Synchronous.
   */
  public static void fetchAndProcessAnnouncements(long lastLaunch,
                                                  AnnouncementsFetchDelegate delegate) {
    final long now = System.currentTimeMillis();
    Logger.debug(LOG_TAG, "Fetching announcements. Last launch: " + lastLaunch + "; now: " + now);
    try {
      final String base = delegate.getServiceURL();
      final URI uri = getAnnounceURI(base, lastLaunch);
      Logger.info(LOG_TAG, "Fetching announcements from " + uri.toASCIIString());
      fetchAnnouncements(uri, delegate);
    } catch (URISyntaxException e) {
      Logger.warn(LOG_TAG, "Couldn't create URL.", e);
      return;
    }
  }
}
