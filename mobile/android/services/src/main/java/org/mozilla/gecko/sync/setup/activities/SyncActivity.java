/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.setup.activities;

import android.app.Activity;
import android.os.Bundle;

/**
 * Shared superclass of Sync activities. Currently exists to prepare per-thread
 * logging.
 */
public abstract class SyncActivity extends Activity {

  @Override
  protected void onResume() {
    super.onResume();
    ActivityUtils.prepareLogging();
  }

  @Override
  public void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    ActivityUtils.prepareLogging();
  }
}
