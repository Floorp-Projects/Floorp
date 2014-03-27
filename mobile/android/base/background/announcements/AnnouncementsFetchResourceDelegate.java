/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.announcements;

import java.io.IOException;
import java.security.GeneralSecurityException;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;

import org.json.simple.JSONArray;
import org.json.simple.JSONObject;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.NonArrayJSONException;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.net.BaseResource;
import org.mozilla.gecko.sync.net.BaseResourceDelegate;
import org.mozilla.gecko.sync.net.Resource;
import org.mozilla.gecko.sync.net.SyncResponse;

import ch.boye.httpclientandroidlib.Header;
import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.client.ClientProtocolException;
import ch.boye.httpclientandroidlib.client.methods.HttpRequestBase;
import ch.boye.httpclientandroidlib.impl.client.DefaultHttpClient;
import ch.boye.httpclientandroidlib.impl.cookie.DateUtils;
import ch.boye.httpclientandroidlib.protocol.HTTP;

/**
 * Converts HTTP resource callbacks into AnnouncementsFetchDelegate callbacks.
 */
public class AnnouncementsFetchResourceDelegate extends BaseResourceDelegate {
  private static final String ACCEPT_HEADER = "application/json;charset=utf-8";

  private static final String LOG_TAG = "AnnounceFetchRD";

  protected final long startTime;
  protected AnnouncementsFetchDelegate delegate;

  public AnnouncementsFetchResourceDelegate(Resource resource, AnnouncementsFetchDelegate delegate) {
    super(resource);
    this.startTime = System.currentTimeMillis();
    this.delegate  = delegate;
  }

  @Override
  public String getUserAgent() {
    return delegate.getUserAgent();
  }

  @Override
  public void addHeaders(HttpRequestBase request, DefaultHttpClient client) {
    super.addHeaders(request, client);

    // The basics.
    request.addHeader("Accept-Language", delegate.getLocale().toString());
    request.addHeader("Accept",          ACCEPT_HEADER);

    // We never want to keep connections alive.
    request.addHeader("Connection", "close");

    // Set If-Modified-Since to avoid re-fetching content.
    final String ifModifiedSince = delegate.getLastDate();
    if (ifModifiedSince != null) {
      Logger.info(LOG_TAG, "If-Modified-Since: " + ifModifiedSince);
      request.addHeader("If-Modified-Since", ifModifiedSince);
    }

    // Just in case.
    request.removeHeaders("Cookie");
  }

  private List<Announcement> parseBody(ExtendedJSONObject body) throws NonArrayJSONException {
    List<Announcement> out = new ArrayList<Announcement>(1);
    JSONArray snippets = body.getArray("announcements");
    if (snippets == null) {
      Logger.warn(LOG_TAG, "Missing announcements body. Returning empty.");
      return out;
    }

    for (Object s : snippets) {
      try {
        out.add(Announcement.parseAnnouncement(new ExtendedJSONObject((JSONObject) s)));
      } catch (Exception e) {
        Logger.warn(LOG_TAG, "Malformed announcement or display failed. Skipping.", e);
      }
    }
    return out;
  }

  @Override
  public void handleHttpResponse(HttpResponse response) {
    final Header dateHeader = response.getFirstHeader(HTTP.DATE_HEADER);
    String date = null;
    if (dateHeader != null) {
      // Note that we are deliberately not validating the server time here.
      // We pass it directly back to the server; we don't care about the
      // contents, and if we reject a value we essentially re-initialize
      // the client, which will cause stale announcements to be re-fetched.
      date = dateHeader.getValue();
    }
    if (date == null) {
      // Use local clock, because skipping is better than re-fetching.
      date = DateUtils.formatDate(new Date());
      Logger.warn(LOG_TAG, "No fetch date; using local time " + date);
    }

    final SyncResponse r = new SyncResponse(response);    // For convenience.
    try {
      final int statusCode = r.getStatusCode();
      Logger.debug(LOG_TAG, "Got announcements response: " + statusCode);

      if (statusCode == 204 || statusCode == 304) {
        BaseResource.consumeEntity(response);
        delegate.onNoNewAnnouncements(startTime, date);
        return;
      }

      if (statusCode == 200) {
        final List<Announcement> snippets;
        try {
          snippets = parseBody(r.jsonObjectBody());
        } catch (Exception e) {
          delegate.onRemoteError(e);
          return;
        }
        delegate.onNewAnnouncements(snippets, startTime, date);
        return;
      }

      if (statusCode == 400 || statusCode == 405) {
        // We did something wrong.
        Logger.warn(LOG_TAG, "We did something wrong. Oh dear.");
        // Fall through.
      }

      if (statusCode == 503 || statusCode == 500) {
        Logger.warn(LOG_TAG, "Server issue: " + r.body());
        delegate.onBackoff(r.retryAfterInSeconds());
        return;
      }

      // Otherwise, clean up.
      delegate.onRemoteFailure(statusCode);

    } catch (Exception e) {
      Logger.warn(LOG_TAG, "Failed to extract body.", e);
      delegate.onRemoteError(e);
    }
  }

  @Override
  public void handleHttpProtocolException(ClientProtocolException e) {
    Logger.warn(LOG_TAG, "Protocol exception.", e);
    delegate.onLocalError(e);
  }

  @Override
  public void handleHttpIOException(IOException e) {
    Logger.warn(LOG_TAG, "IO exception.", e);
    delegate.onLocalError(e);
  }

  @Override
  public void handleTransportException(GeneralSecurityException e) {
    Logger.warn(LOG_TAG, "Transport exception.", e);
    // Class this as a remote error, because it's probably something odd
    // with SSL negotiation.
    delegate.onRemoteError(e);
  }

  /**
   * Be very thorough in case the superclass implementation changes.
   * We never want this to be an authenticated request.
   */
  @Override
  public AuthHeaderProvider getAuthHeaderProvider() {
    return null;
  }
}
