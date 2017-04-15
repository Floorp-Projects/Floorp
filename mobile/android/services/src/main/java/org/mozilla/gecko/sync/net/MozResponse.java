/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.net;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.Reader;
import java.util.Scanner;

import org.json.simple.JSONArray;
import org.json.simple.parser.JSONParser;
import org.json.simple.parser.ParseException;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.NonArrayJSONException;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.util.IOUtils;
import org.mozilla.gecko.util.StringUtils;

import ch.boye.httpclientandroidlib.Header;
import ch.boye.httpclientandroidlib.HttpEntity;
import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.HttpStatus;
import ch.boye.httpclientandroidlib.impl.cookie.DateParseException;
import ch.boye.httpclientandroidlib.impl.cookie.DateUtils;

public class MozResponse {
  private static final String LOG_TAG = "MozResponse";

  private static final String HEADER_RETRY_AFTER = "retry-after";

  protected HttpResponse response;
  private String body = null;

  public HttpResponse httpResponse() {
    return this.response;
  }

  public int getStatusCode() {
    return this.response.getStatusLine().getStatusCode();
  }

  public boolean wasSuccessful() {
    return this.getStatusCode() == 200;
  }

  public boolean isInvalidAuthentication() {
    return this.getStatusCode() == HttpStatus.SC_UNAUTHORIZED;
  }

  /**
   * Fetch the content type of the HTTP response body.
   *
   * @return a <code>Header</code> instance, or <code>null</code> if there was
   *         no body or no valid Content-Type.
   */
  public Header getContentType() {
    HttpEntity entity = this.response.getEntity();
    if (entity == null) {
      return null;
    }
    return entity.getContentType();
  }

  private static boolean missingHeader(String value) {
    return value == null ||
           value.trim().length() == 0;
  }

  public String body() throws IllegalStateException, IOException {
    if (body != null) {
      return body;
    }
    final HttpEntity entity = this.response.getEntity();
    if (entity == null) {
      body = null;
      return null;
    }

    InputStreamReader is = new InputStreamReader(entity.getContent(), StringUtils.UTF_8);
    // Oh, Java, you are so evil.
    body = new Scanner(is).useDelimiter("\\A").next();
    return body;
  }

  /**
   * Return the body as a <b>non-null</b> <code>ExtendedJSONObject</code>.
   *
   * @return A non-null <code>ExtendedJSONObject</code>.
   *
   * @throws IllegalStateException
   * @throws IOException
   * @throws NonObjectJSONException
   */
  public ExtendedJSONObject jsonObjectBody() throws IllegalStateException, IOException, NonObjectJSONException {
    if (body != null) {
      // Do it from the cached String.
      return new ExtendedJSONObject(body);
    }

    HttpEntity entity = this.response.getEntity();
    if (entity == null) {
      throw new IOException("no entity");
    }

    Reader in = null;
    try {
      in = new BufferedReader(new InputStreamReader(entity.getContent(), StringUtils.UTF_8));
      return new ExtendedJSONObject(in);
    } finally {
      IOUtils.safeStreamClose(in);
    }
  }

  public JSONArray jsonArrayBody() throws NonArrayJSONException, IOException {
    final JSONParser parser = new JSONParser();
    try {
      if (body != null) {
        // Do it from the cached String.
        return (JSONArray) parser.parse(body);
      }

      final HttpEntity entity = this.response.getEntity();
      if (entity == null) {
        throw new IOException("no entity");
      }

      final InputStream content = entity.getContent();
      final Reader in = new BufferedReader(new InputStreamReader(content, "UTF-8"));
      try {
        return (JSONArray) parser.parse(in);
      } finally {
        in.close();
      }
    } catch (ClassCastException | ParseException e) {
      NonArrayJSONException exception = new NonArrayJSONException("value must be a json array");
      exception.initCause(e);
      throw exception;
    }
  }

  protected boolean hasHeader(String h) {
    return this.response.containsHeader(h);
  }

  public MozResponse(HttpResponse res) {
    response = res;
  }

  protected String getNonMissingHeader(String h) {
    if (!this.hasHeader(h)) {
      return null;
    }

    final Header header = this.response.getFirstHeader(h);
    final String value = header.getValue();
    if (missingHeader(value)) {
      Logger.warn(LOG_TAG, h + " header present but empty.");
      return null;
    }
    return value;
  }

  protected long getLongHeader(String h) throws NumberFormatException {
    final String value = getNonMissingHeader(h);
    if (value == null) {
      return -1L;
    }
    return Long.parseLong(value, 10);
  }

  protected int getIntegerHeader(String h) throws NumberFormatException {
    final String value = getNonMissingHeader(h);
    if (value == null) {
      return -1;
    }
    return Integer.parseInt(value, 10);
  }

  /**
   * @return A number of seconds, or -1 if the 'Retry-After' header was not present.
   */
  public int retryAfterInSeconds() throws NumberFormatException {
    final String retryAfter = getNonMissingHeader(HEADER_RETRY_AFTER);
    if (retryAfter == null) {
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
      Logger.warn(LOG_TAG, "Retry-After header neither integer nor date: " + retryAfter);
      return -1;
    }
  }

  /**
   * @return A number of seconds, or -1 if the 'Backoff' header was not
   *         present.
   */
  public int backoffInSeconds() throws NumberFormatException {
    return this.getIntegerHeader("backoff");
  }

  public void logResponseBody(final String logTag) {
    if (!Logger.LOG_PERSONAL_INFORMATION) {
      return;
    }
    try {
      Logger.pii(logTag, "Response body: " + body());
    } catch (Throwable e) {
      Logger.debug(logTag, "No response body.");
    }
  }
}
