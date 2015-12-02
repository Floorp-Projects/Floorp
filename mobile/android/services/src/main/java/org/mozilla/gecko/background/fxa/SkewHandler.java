/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.fxa;

import java.net.URI;
import java.net.URISyntaxException;
import java.util.HashMap;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.net.Resource;

import ch.boye.httpclientandroidlib.Header;
import ch.boye.httpclientandroidlib.HttpHeaders;
import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.impl.cookie.DateParseException;
import ch.boye.httpclientandroidlib.impl.cookie.DateUtils;

public class SkewHandler {
  private static final String LOG_TAG = "SkewHandler";
  protected volatile long skewMillis = 0L;
  protected final String hostname;

  private static final HashMap<String, SkewHandler> skewHandlers = new HashMap<String, SkewHandler>();

  public static SkewHandler getSkewHandlerForResource(final Resource resource) {
    return getSkewHandlerForHostname(resource.getHostname());
  }

  public static SkewHandler getSkewHandlerFromEndpointString(final String url) throws URISyntaxException {
    if (url == null) {
      throw new IllegalArgumentException("url must not be null.");
    }
    URI u = new URI(url);
    return getSkewHandlerForHostname(u.getHost());
  }

  public static synchronized SkewHandler getSkewHandlerForHostname(final String hostname) {
    SkewHandler handler = skewHandlers.get(hostname);
    if (handler == null) {
      handler = new SkewHandler(hostname);
      skewHandlers.put(hostname, handler);
    }
    return handler;
  }

  public static synchronized void clearSkewHandlers() {
    skewHandlers.clear();
  }

  public SkewHandler(final String hostname) {
    this.hostname = hostname;
  }

  public boolean updateSkewFromServerMillis(long millis, long now) {
    skewMillis = millis - now;
    Logger.debug(LOG_TAG, "Updated skew: " + skewMillis + "ms for hostname " + this.hostname);
    return true;
  }

  public boolean updateSkewFromHTTPDateString(String date, long now) {
    try {
      final long millis = DateUtils.parseDate(date).getTime();
      return updateSkewFromServerMillis(millis, now);
    } catch (DateParseException e) {
      Logger.warn(LOG_TAG, "Unexpected: invalid Date header from " + this.hostname);
      return false;
    }
  }

  public boolean updateSkewFromDateHeader(Header header, long now) {
    String date = header.getValue();
    if (null == date) {
      Logger.warn(LOG_TAG, "Unexpected: null Date header from " + this.hostname);
      return false;
    }
    return updateSkewFromHTTPDateString(date, now);
  }

  /**
   * Update our tracked skew value to account for the local clock differing from
   * the server's.
   * 
   * @param response
   *          the received HTTP response.
   * @param now
   *          the current time in milliseconds.
   * @return true if the skew value was updated, false otherwise.
   */
  public boolean updateSkew(HttpResponse response, long now) {
    Header header = response.getFirstHeader(HttpHeaders.DATE);
    if (null == header) {
      Logger.warn(LOG_TAG, "Unexpected: missing Date header from " + this.hostname);
      return false;
    }
    return updateSkewFromDateHeader(header, now);
  }

  public long getSkewInMillis() {
    return skewMillis;
  }

  public long getSkewInSeconds() {
    return skewMillis / 1000;
  }

  public void resetSkew() {
    skewMillis = 0L;
  }
}