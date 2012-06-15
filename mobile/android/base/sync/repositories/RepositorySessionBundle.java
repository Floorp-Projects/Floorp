/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories;

import java.io.IOException;

import org.json.simple.parser.ParseException;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.NonObjectJSONException;

public class RepositorySessionBundle extends ExtendedJSONObject {

  private static final String LOG_TAG = "RepositorySessionBundle";

  public RepositorySessionBundle() {
    super();
  }

  public RepositorySessionBundle(String jsonString) throws IOException, ParseException, NonObjectJSONException {
    super(jsonString);
  }

  public RepositorySessionBundle(long lastSyncTimestamp) {
    this();
    this.setTimestamp(lastSyncTimestamp);
  }

  public long getTimestamp() {
    if (this.containsKey("timestamp")) {
      return this.getLong("timestamp");
    }
    return -1;
  }

  public void setTimestamp(long timestamp) {
    Logger.debug(LOG_TAG, "Setting timestamp on RepositorySessionBundle to " + timestamp);
    this.put("timestamp", new Long(timestamp));
  }

  public void bumpTimestamp(long timestamp) {
    long existing = this.getTimestamp();
    if (timestamp > existing) {
      this.setTimestamp(timestamp);
    } else {
      Logger.debug(LOG_TAG, "Timestamp " + timestamp + " not greater than " + existing + "; not bumping.");
    }
  }
}
