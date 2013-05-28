/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.healthreport;

import java.io.File;
import java.util.HashMap;

import org.mozilla.gecko.background.common.log.Logger;

import android.content.Context;

/**
 * Manages a set of per-profile Health Report storage helpers.
 */
public class HealthReportDatabases {
  private static final String LOG_TAG = "HealthReportDatabases";

  private Context context;
  private final HashMap<File, HealthReportDatabaseStorage> storages = new HashMap<File, HealthReportDatabaseStorage>();


  public HealthReportDatabases(final Context context) {
    this.context = context;
  }

  public synchronized HealthReportDatabaseStorage getDatabaseHelperForProfile(final File profileDir) {
    if (profileDir == null) {
      throw new IllegalArgumentException("No profile provided.");
    }

    if (this.storages.containsKey(profileDir)) {
      return this.storages.get(profileDir);
    }

    final HealthReportDatabaseStorage helper;
    helper = new HealthReportDatabaseStorage(this.context, profileDir);
    this.storages.put(profileDir, helper);
    return helper;
  }

  public synchronized void closeDatabaseHelpers() {
    for (HealthReportDatabaseStorage helper : storages.values()) {
      try {
        helper.close();
      } catch (Exception e) {
        Logger.warn(LOG_TAG, "Failed to close database helper.", e);
      }
    }
    storages.clear();
  }
}
