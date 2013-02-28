/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.setup.activities;

import org.mozilla.gecko.R;
import org.mozilla.gecko.background.common.GlobalConstants;
import org.mozilla.gecko.sync.setup.Constants;
import org.mozilla.gecko.sync.setup.SyncAccounts;

import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.TextView;

public class SetupSuccessActivity extends SyncActivity {
  private TextView setupSubtitle;

  @Override
  public void onCreate(Bundle savedInstanceState) {
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
    super.onDestroy();
  }

  /* Click Handlers */
  public void settingsClickHandler(View target) {
    SyncAccounts.openSyncSettings(this);
  }

  public void launchBrowser(View target) {
    Intent intent = new Intent(Intent.ACTION_MAIN);
    intent.setClassName(GlobalConstants.BROWSER_INTENT_PACKAGE, GlobalConstants.BROWSER_INTENT_CLASS);
    startActivity(intent);
  }
}
