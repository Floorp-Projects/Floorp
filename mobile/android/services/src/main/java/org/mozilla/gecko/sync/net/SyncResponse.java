/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.net;

import org.mozilla.gecko.sync.Utils;

import ch.boye.httpclientandroidlib.HttpResponse;

public class SyncResponse extends MozResponse {
  public SyncResponse(HttpResponse res) {
    super(res);
  }

  /**
   * @return A number of seconds, or -1 if the 'X-Weave-Backoff' header was not
   *         present.
   */
  public int weaveBackoffInSeconds() throws NumberFormatException {
    return this.getIntegerHeader("x-weave-backoff");
  }

  /**
   * @return A number of seconds, or -1 if the 'X-Backoff' header was not
   *         present.
   */
  public int xBackoffInSeconds() throws NumberFormatException {
    return this.getIntegerHeader("x-backoff");
  }

  /**
   * Extract a number of seconds, or -1 if none of the specified headers were present.
   *
   * @param includeRetryAfter
   *          if <code>true</code>, the Retry-After header is excluded. This is
   *          useful for processing non-error responses where a Retry-After
   *          header would be unexpected.
   * @return the maximum of the three possible backoff headers, in seconds.
   */
  public int totalBackoffInSeconds(boolean includeRetryAfter) {
    int retryAfterInSeconds = -1;
    if (includeRetryAfter) {
      try {
        retryAfterInSeconds = retryAfterInSeconds();
      } catch (NumberFormatException e) {
      }
    }

    int weaveBackoffInSeconds = -1;
    try {
      weaveBackoffInSeconds = weaveBackoffInSeconds();
    } catch (NumberFormatException e) {
    }

    int backoffInSeconds = -1;
    try {
      backoffInSeconds = xBackoffInSeconds();
    } catch (NumberFormatException e) {
    }

    int totalBackoff = Math.max(retryAfterInSeconds, Math.max(backoffInSeconds, weaveBackoffInSeconds));
    if (totalBackoff < 0) {
      return -1;
    } else {
      return totalBackoff;
    }
  }

  /**
   * @return A number of milliseconds, or -1 if neither the 'Retry-After',
   *         'X-Backoff', or 'X-Weave-Backoff' header were present.
   */
  public long totalBackoffInMilliseconds() {
    long totalBackoff = totalBackoffInSeconds(true);
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
