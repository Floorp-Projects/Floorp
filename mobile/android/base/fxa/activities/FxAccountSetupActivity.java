/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.activities;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.R;
import org.mozilla.gecko.background.common.log.Logger;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

/**
 * Activity which displays login screen to the user.
 */
public class FxAccountSetupActivity extends Activity {
  protected static final String LOG_TAG = FxAccountSetupActivity.class.getSimpleName();

  /**
   * {@inheritDoc}
   */
  @Override
  public void onCreate(Bundle icicle) {
    Logger.debug(LOG_TAG, "onCreate(" + icicle + ")");

    super.onCreate(icicle);
    setContentView(R.layout.fxaccount_setup);
  }

  @Override
  public void onResume() {
    Logger.debug(LOG_TAG, "onResume()");

    super.onResume();

    // Start Fennec at about:accounts page.
    Intent intent = new Intent(Intent.ACTION_VIEW);
    intent.setClassName(AppConstants.ANDROID_PACKAGE_NAME,
        AppConstants.ANDROID_PACKAGE_NAME + ".App");
    intent.setData(Uri.parse("about:accounts"));

    startActivity(intent);
    finish();
  }
}
