/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.announcements;

import java.io.FileDescriptor;
import java.io.PrintWriter;
import java.net.URI;
import java.util.List;
import java.util.Locale;

import org.mozilla.gecko.BrowserLocaleManager;
import org.mozilla.gecko.background.BackgroundService;
import org.mozilla.gecko.background.common.GlobalConstants;
import org.mozilla.gecko.background.common.log.Logger;

import android.content.Intent;
import android.content.SharedPreferences;
import android.os.IBinder;

/**
 * A Service to periodically check for new published announcements,
 * presenting them to the user if local conditions permit.
 *
 * We extend IntentService, rather than just Service, because this gives us
 * a worker thread to avoid main-thread networking.
 *
 * Yes, even though we're in an alarm-triggered service, it still counts
 * as main-thread.
 *
 * The operation of this service is as follows:
 *
 * 0. Decide if a request should be made.
 * 1. Compute the arguments to the request. This includes enough
 *    pertinent details to allow the server to pre-filter a message
 *    set, recording enough tracking details to compute statistics.
 * 2. Issue the request. If this succeeds with a 200 or 204, great;
 *    track that timestamp for the next run through Step 0.
 * 3. Process any received messages.
 *
 * Message processing is as follows:
 *
 * 0. Decide if message display should occur. This might involve
 *    user preference or other kinds of environmental factors.
 * 1. Use the AnnouncementPresenter to open the announcement.
 *
 * Future:
 * * Persisting of multiple announcements.
 * * Prioritization.
 */
public class AnnouncementsService extends BackgroundService implements AnnouncementsFetchDelegate {
  private static final String WORKER_THREAD_NAME = "AnnouncementsServiceWorker";
  private static final String LOG_TAG = "AnnounceService";

  public AnnouncementsService() {
    super(WORKER_THREAD_NAME);
    Logger.setThreadLogTag(AnnouncementsConstants.GLOBAL_LOG_TAG);
    Logger.debug(LOG_TAG, "Creating AnnouncementsService.");
  }

  public boolean shouldFetchAnnouncements() {
    final long now = System.currentTimeMillis();

    if (!backgroundDataIsEnabled()) {
      Logger.debug(LOG_TAG, "Background data not possible. Skipping.");
      return false;
    }

    // Don't fetch if we were told to back off.
    if (getEarliestNextFetch() > now) {
      return false;
    }

    // Don't do anything if we haven't waited long enough.
    final long lastFetch = getLastFetch();

    // Just in case the alarm manager schedules us more frequently, or something
    // goes awry with relaunches.
    if ((now - lastFetch) < AnnouncementsConstants.MINIMUM_FETCH_INTERVAL_MSEC) {
      Logger.debug(LOG_TAG, "Returning: minimum fetch interval of " + AnnouncementsConstants.MINIMUM_FETCH_INTERVAL_MSEC + "ms not met.");
      return false;
    }

    return true;
  }

  /**
   * Display the first valid announcement in the list.
   */
  protected void processAnnouncements(final List<Announcement> announcements) {
    if (announcements == null) {
      Logger.warn(LOG_TAG, "No announcements to present.");
      return;
    }

    boolean presented = false;
    for (Announcement an : announcements) {
      // Do this so we at least log, rather than just returning.
      if (presented) {
        Logger.warn(LOG_TAG, "Skipping announcement \"" + an.getTitle() + "\": one already shown.");
        continue;
      }
      if (Announcement.isValidAnnouncement(an)) {
        presented = true;
        AnnouncementPresenter.displayAnnouncement(this, an);
      }
    }
  }

  /**
   * If it's time to do a fetch -- we've waited long enough,
   * we're allowed to use background data, etc. -- then issue
   * a fetch. The subsequent background check is handled implicitly
   * by the AlarmManager.
   */
  @Override
  public void onHandleIntent(Intent intent) {
    Logger.setThreadLogTag(AnnouncementsConstants.GLOBAL_LOG_TAG);
    Logger.debug(LOG_TAG, "Running AnnouncementsService.");

    if (AnnouncementsConstants.DISABLED) {
      Logger.debug(LOG_TAG, "Announcements disabled. Returning from AnnouncementsService.");
      return;
    }

    if (!shouldFetchAnnouncements()) {
      Logger.debug(LOG_TAG, "Not fetching.");
      return;
    }

    // Ensure that our locale is up to date, so that the fetcher's
    // Accept-Language header is, too.
    BrowserLocaleManager.getInstance().getAndApplyPersistedLocale(getApplicationContext());

    // Otherwise, grab our announcements URL and process the contents.
    AnnouncementsFetcher.fetchAndProcessAnnouncements(getLastLaunch(), this);
  }

  @Override
  public IBinder onBind(Intent intent) {
    return null;
  }

  protected long getLastLaunch() {
    return getSharedPreferences().getLong(AnnouncementsConstants.PREF_LAST_LAUNCH, 0);
  }

  protected SharedPreferences getSharedPreferences() {
    return this.getSharedPreferences(AnnouncementsConstants.PREFS_BRANCH, GlobalConstants.SHARED_PREFERENCES_MODE);
  }

  @Override
  protected void dump(FileDescriptor fd, PrintWriter writer, String[] args) {
    super.dump(fd, writer, args);

    final long lastFetch = getLastFetch();
    final long lastLaunch = getLastLaunch();
    writer.write("AnnouncementsService: last fetch " + lastFetch +
                 ", last Firefox activity: " + lastLaunch + "\n");
  }

  protected void setEarliestNextFetch(final long earliestInMsec) {
    this.getSharedPreferences().edit().putLong(AnnouncementsConstants.PREF_EARLIEST_NEXT_ANNOUNCE_FETCH, earliestInMsec).commit();
  }

  protected long getEarliestNextFetch() {
    return this.getSharedPreferences().getLong(AnnouncementsConstants.PREF_EARLIEST_NEXT_ANNOUNCE_FETCH, 0L);
  }

  protected void setLastFetch(final long fetch) {
    this.getSharedPreferences().edit().putLong(AnnouncementsConstants.PREF_LAST_FETCH_LOCAL_TIME, fetch).commit();
  }

  @Override
  public long getLastFetch() {
    return this.getSharedPreferences().getLong(AnnouncementsConstants.PREF_LAST_FETCH_LOCAL_TIME, 0L);
  }

  protected String setLastDate(final String fetch) {
    if (fetch == null) {
      this.getSharedPreferences().edit().remove(AnnouncementsConstants.PREF_LAST_FETCH_SERVER_DATE).commit();
      return null;
    }
    this.getSharedPreferences().edit().putString(AnnouncementsConstants.PREF_LAST_FETCH_SERVER_DATE, fetch).commit();
    return fetch;
  }

  @Override
  public String getLastDate() {
    return this.getSharedPreferences().getString(AnnouncementsConstants.PREF_LAST_FETCH_SERVER_DATE, null);
  }

  /**
   * Use this to write the persisted server URL, overriding
   * the default value.
   * @param url a URI identifying the full request path, e.g.,
   *            "http://foo.com:1234/announce/"
   */
  public void setAnnouncementsServerBaseURL(final URI url) {
    if (url == null) {
      throw new IllegalArgumentException("url cannot be null.");
    }
    final String scheme = url.getScheme();
    if (scheme == null) {
      throw new IllegalArgumentException("url must have a scheme.");
    }
    if (!scheme.equalsIgnoreCase("http") && !scheme.equalsIgnoreCase("https")) {
      throw new IllegalArgumentException("url must be http or https.");
    }
    SharedPreferences p = this.getSharedPreferences();
    p.edit().putString(AnnouncementsConstants.PREF_ANNOUNCE_SERVER_BASE_URL, url.toASCIIString()).commit();
  }

  /**
   * Return the service URL, including protocol version and application identifier. E.g.,
   *
   *   "https://campaigns.services.mozilla.com/announce/1/android/"
   */
  @Override
  public String getServiceURL() {
    SharedPreferences p = this.getSharedPreferences();
    String base = p.getString(AnnouncementsConstants.PREF_ANNOUNCE_SERVER_BASE_URL, AnnouncementsConstants.DEFAULT_ANNOUNCE_SERVER_BASE_URL);
    return base + AnnouncementsConstants.ANNOUNCE_PATH_SUFFIX;
  }

  @Override
  public Locale getLocale() {
    return Locale.getDefault();
  }

  @Override
  public String getUserAgent() {
    return AnnouncementsConstants.USER_AGENT;
  }

  protected void persistTimes(long fetched, String date) {
    setLastFetch(fetched);
    if (date != null) {
      setLastDate(date);
    }
  }

  @Override
  public void onNoNewAnnouncements(long fetched, String date) {
    Logger.info(LOG_TAG, "No new announcements to display.");
    persistTimes(fetched, date);
  }

  @Override
  public void onNewAnnouncements(List<Announcement> announcements, long fetched, String date) {
    Logger.info(LOG_TAG, "Processing announcements: " + announcements.size());
    persistTimes(fetched, date);
    processAnnouncements(announcements);
  }

  @Override
  public void onRemoteFailure(int status) {
    // Bump our fetch timestamp.
    Logger.warn(LOG_TAG, "Got remote fetch status " + status + "; bumping fetch time.");
    setLastFetch(System.currentTimeMillis());
  }

  @Override
  public void onRemoteError(Exception e) {
    // Bump our fetch timestamp.
    Logger.warn(LOG_TAG, "Error processing response.", e);
    setLastFetch(System.currentTimeMillis());
  }

  @Override
  public void onLocalError(Exception e) {
    Logger.error(LOG_TAG, "Got exception in fetch.", e);
    // Do nothing yet, so we'll retry.
  }

  @Override
  public void onBackoff(int retryAfterInSeconds) {
    Logger.info(LOG_TAG, "Got retry after: " + retryAfterInSeconds);
    final long delayInMsec = Math.max(retryAfterInSeconds * 1000, AnnouncementsConstants.DEFAULT_BACKOFF_MSEC);
    final long fuzzedBackoffInMsec = delayInMsec + Math.round(((double) delayInMsec * 0.25d * Math.random()));
    Logger.debug(LOG_TAG, "Fuzzed backoff: " + fuzzedBackoffInMsec + "ms.");
    setEarliestNextFetch(fuzzedBackoffInMsec + System.currentTimeMillis());
  }
}
