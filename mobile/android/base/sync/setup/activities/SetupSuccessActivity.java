/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.setup.activities;

import org.mozilla.gecko.R;
import org.mozilla.gecko.sync.GlobalConstants;
import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.setup.Constants;
import org.mozilla.gecko.sync.setup.SyncAccounts;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.TextView;

public class SetupSuccessActivity extends Activity {
  private final static String LOG_TAG = "SetupSuccessActivity";
  private TextView setupSubtitle;

  @Override
  public void onCreate(Bundle savedInstanceState) {
    setTheme(R.style.SyncTheme);
    super.onCreate(savedInstanceState);
    Bundle extras = this.getIntent().getExtras();
    setContentView(R.layout.sync_setup_success);
    setupSubtitle = ((TextView) findViewById(R.id.setup_success_subtitle));
    if (extras != null) {
      boolean isSetup = extras.getBoolean(Constants.INTENT_EXTRA_IS_SETUP);
      if (!isSetup) {
        setupSubtitle.setText(getString(R.string.sync_subtitle_manage));
      }
    }
  }

  @Override
  public void onDestroy() {
    Logger.debug(LOG_TAG, "onDestroy() called.");
    super.onDestroy();
  }

  /* Click Handlers */
  public void settingsClickHandler(View target) {
    SyncAccounts.openSyncSettings(this);
  }


  public void launchBrowser(View target) {
    Intent intent = new Intent(Intent.ACTION_MAIN);
    intent.setClassName(GlobalConstants.BROWSER_INTENT_PACKAGE, GlobalConstants.BROWSER_INTENT_CLASS);
    // Start Fennec as a new task. If Fennec is already running, bring it to the front.
    intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
    startActivity(intent);
  }
}
