/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.net;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.Scanner;

import org.json.simple.parser.ParseException;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.sync.Utils;

import android.util.Log;
import ch.boye.httpclientandroidlib.Header;
import ch.boye.httpclientandroidlib.HttpEntity;
import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.impl.cookie.DateParseException;
import ch.boye.httpclientandroidlib.impl.cookie.DateUtils;

public class SyncResponse {
  private static final String HEADER_RETRY_AFTER = "retry-after";
  private static final String LOG_TAG = "SyncResponse";

  protected HttpResponse response;

  public SyncResponse() {
    super();
  }

  public SyncResponse(HttpResponse res) {
    response = res;
  }

  public HttpResponse httpResponse() {
    return this.response;
  }

  public int getStatusCode() {
    return this.response.getStatusLine().getStatusCode();
  }

  public boolean wasSuccessful() {
    return this.getStatusCode() == 200;
  }

  private String body = null;
  public String body() throws IllegalStateException, IOException {
    if (body != null) {
      return body;
    }
    InputStreamReader is = new InputStreamReader(this.response.getEntity().getContent());
    // Oh, Java, you are so evil.
    body = new Scanner(is).useDelimiter("\\A").next();
    return body;
  }

  /**
   * Return the body as an Object.
   *
   * @return null if there is no body, or an Object if it successfully parses.
   *         The return value will be an ExtendedJSONObject if it's a JSON object.
   * @throws IllegalStateException
   * @throws IOException
   * @throws ParseException
   */
  public Object jsonBody() throws IllegalStateException, IOException,
                          ParseException {
    if (body != null) {
      // Do it from the cached String.
      return ExtendedJSONObject.parse(body);
    }

    HttpEntity entity = this.response.getEntity();
    if (entity == null) {
      return null;
    }
    InputStream content = entity.getContent();
    try {
      return ExtendedJSONObject.parse(content);
    } finally {
      content.close();
    }
  }

  /**
   * Return the body as a <b>non-null</b> <code>ExtendedJSONObject</code>.
   *
   * @return A non-null <code>ExtendedJSONObject</code>.
   *
   * @throws IllegalStateException
   * @throws IOException
   * @throws ParseException
   * @throws NonObjectJSONException
   */
  public ExtendedJSONObject jsonObjectBody() throws IllegalStateException,
                                            IOException, ParseException,
                                            NonObjectJSONException {
    Object body = this.jsonBody();
    if (body instanceof ExtendedJSONObject) {
      return (ExtendedJSONObject) body;
    }
    throw new NonObjectJSONException(body);
  }

  private boolean hasHeader(String h) {
    return this.response.containsHeader(h);
  }

  private static boolean missingHeader(String value) {
    return value == null ||
           value.trim().length() == 0;
  }

  private int getIntegerHeader(String h) throws NumberFormatException {
    if (this.hasHeader(h)) {
      Header header = this.response.getFirstHeader(h);
      String value  = header.getValue();
      if (missingHeader(value)) {
        Log.w(LOG_TAG, h + " header present but empty.");
        return -1;
      }
      return Integer.parseInt(value, 10);
    }
    return -1;
  }

  /**
   * @return A number of seconds, or -1 if the 'Retry-After' header was not present.
   */
  public int retryAfterInSeconds() throws NumberFormatException {
    if (!this.hasHeader(HEADER_RETRY_AFTER)) {
      return -1;
    }

    Header header = this.response.getFirstHeader(HEADER_RETRY_AFTER);
    String retryAfter = header.getValue();
    if (missingHeader(retryAfter)) {
      Log.w(LOG_TAG, "Retry-After header present but empty.");
      return -1;
    }

    try {
      return Integer.parseInt(retryAfter, 10);
    } catch (NumberFormatException e) {
      // Fall through to try date format.
    }

    try {
      final long then = DateUtils.parseDate(retryAfter).getTime();
      final long now  = System.currentTimeMillis();
      return (int)((then - now) / 1000);     // Convert milliseconds to seconds.
    } catch (DateParseException e) {
      Log.w(LOG_TAG, "Retry-After header neither integer nor date: " + retryAfter);
      return -1;
    }
  }

  /**
   * @return A number of seconds, or -1 if the 'X-Weave-Backoff' header was not
   *         present.
   */
  public int weaveBackoffInSeconds() throws NumberFormatException {
    return this.getIntegerHeader("x-weave-backoff");
  }

  /**
   * @return A number of milliseconds, or -1 if neither the 'Retry-After' or
   *         'X-Weave-Backoff' header was present.
   */
  public long totalBackoffInMilliseconds() {
    int retryAfterInSeconds = -1;
    try {
      retryAfterInSeconds = retryAfterInSeconds();
    } catch (NumberFormatException e) {
    }

    int weaveBackoffInSeconds = -1;
    try {
      weaveBackoffInSeconds = weaveBackoffInSeconds();
    } catch (NumberFormatException e) {
    }

    long totalBackoff = (long) Math.max(retryAfterInSeconds, weaveBackoffInSeconds);
    if (totalBackoff < 0) {
      return -1;
    } else {
      return 1000 * totalBackoff;
    }
  }

  /**
   * The timestamp returned from a Sync server is a decimal number of seconds,
   * e.g., 1323393518.04.
   *
   * We want milliseconds since epoch.
   *
   * @return milliseconds since the epoch, as a long, or -1 if the header
   *         was missing or invalid.
   */
  public long normalizedWeaveTimestamp() {
    String h = "x-weave-timestamp";
    if (!this.hasHeader(h)) {
      return -1;
    }

    return Utils.decimalSecondsToMilliseconds(this.response.getFirstHeader(h).getValue());
  }

  public int weaveRecords() throws NumberFormatException {
    return this.getIntegerHeader("x-weave-records");
  }

  public int weaveQuotaRemaining() throws NumberFormatException {
    return this.getIntegerHeader("x-weave-quota-remaining");
  }

  public String weaveAlert() {
    if (this.hasHeader("x-weave-alert")) {
      return this.response.getFirstHeader("x-weave-alert").getValue();
    }
    return null;
  }
}
