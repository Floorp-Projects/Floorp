/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.activities;

import org.mozilla.gecko.R;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.authenticator.FxAccountAuthenticator;

import android.accounts.AccountAuthenticatorActivity;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.view.View.OnClickListener;

/**
 * Activity which displays sign up/sign in screen to the user.
 */
public class FxAccountGetStartedActivity extends AccountAuthenticatorActivity {
  protected static final String LOG_TAG = FxAccountGetStartedActivity.class.getSimpleName();

  private static final int CHILD_REQUEST_CODE = 1;

  /**
   * {@inheritDoc}
   */
  @Override
  public void onCreate(Bundle icicle) {
    Logger.setThreadLogTag(FxAccountConstants.GLOBAL_LOG_TAG);
    Logger.debug(LOG_TAG, "onCreate(" + icicle + ")");

    super.onCreate(icicle);
    setContentView(R.layout.fxaccount_get_started);

    // linkifyTextViews(null, new int[] { R.id.old_firefox });

    View button = findViewById(R.id.get_started_button);
    button.setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View v) {
        Intent intent = new Intent(FxAccountGetStartedActivity.this, FxAccountCreateAccountActivity.class);
        // Per http://stackoverflow.com/a/8992365, this triggers a known bug with
        // the soft keyboard not being shown for the started activity. Why, Android, why?
        intent.setFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
        startActivityForResult(intent, CHILD_REQUEST_CODE);
      }
    });
  }

  public void onResume() {
    super.onResume();
    if (FxAccountAuthenticator.getFirefoxAccounts(this).length > 0) {
      this.setAccountAuthenticatorResult(null);
      setResult(RESULT_CANCELED);
      Intent intent = new Intent(this, FxAccountStatusActivity.class);
      // Per http://stackoverflow.com/a/8992365, this triggers a known bug with
      // the soft keyboard not being shown for the started activity. Why, Android, why?
      intent.setFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
      startActivity(intent);
      finish();
      return;
    }
  }

  /**
   * We started the CreateAccount activity for a result; this returns it to the
   * authenticator.
   */
  @Override
  public void onActivityResult(int requestCode, int resultCode, Intent data) {
    Logger.debug(LOG_TAG, "onActivityResult: " + requestCode);
    if (requestCode != CHILD_REQUEST_CODE) {
      super.onActivityResult(requestCode, resultCode, data);
      return;
    }
    if (data != null) {
      this.setAccountAuthenticatorResult(data.getExtras());
    }
    this.setResult(requestCode, data);
  }
}
