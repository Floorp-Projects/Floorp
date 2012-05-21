/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.setup.activities;

import org.mozilla.gecko.R;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;

public class SetupFailureActivity extends Activity {
  private Context mContext;

  @Override
  public void onCreate(Bundle savedInstanceState) {
    setTheme(R.style.SyncTheme);
    super.onCreate(savedInstanceState);
    setContentView(R.layout.sync_setup_failure);
    mContext = this.getApplicationContext();
  }

  public void manualClickHandler(View target) {
    Intent intent = new Intent(mContext, AccountActivity.class);
    startActivity(intent);
    overridePendingTransition(0, 0);
    finish();
  }

  public void tryAgainClickHandler(View target) {
    finish();
  }

  public void cancelClickHandler(View target) {
    setResult(RESULT_CANCELED);
    finish();
  }
}
