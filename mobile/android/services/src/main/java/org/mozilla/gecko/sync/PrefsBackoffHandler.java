/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;

public class PrefsBackoffHandler implements BackoffHandler {
  public static final String PREF_EARLIEST_NEXT = "earliestnext";

  private final SharedPreferences prefs;
  private final String prefEarliest;

  public PrefsBackoffHandler(final SharedPreferences prefs, final String prefSuffix) {
    if (prefs == null) {
      throw new IllegalArgumentException("prefs must not be null.");
    }
    this.prefs = prefs;
    this.prefEarliest = PREF_EARLIEST_NEXT + "." + prefSuffix;
  }

  @Override
  public synchronized long getEarliestNextRequest() {
    return prefs.getLong(prefEarliest, 0);
  }

  @Override
  public synchronized void setEarliestNextRequest(final long next) {
    final Editor edit = prefs.edit();
    edit.putLong(prefEarliest, next);
    edit.commit();
  }

  @Override
  public synchronized void extendEarliestNextRequest(final long next) {
    if (prefs.getLong(prefEarliest, 0) >= next) {
      return;
    }
    final Editor edit = prefs.edit();
    edit.putLong(prefEarliest, next);
    edit.commit();
  }

  /**
   * Return the number of milliseconds until we're allowed to touch the server again,
   * or 0 if now is fine.
   */
  @Override
  public long delayMilliseconds() {
    long earliestNextRequest = getEarliestNextRequest();
    if (earliestNextRequest <= 0) {
      return 0;
    }
    long now = System.currentTimeMillis();
    return Math.max(0, earliestNextRequest - now);
  }
}