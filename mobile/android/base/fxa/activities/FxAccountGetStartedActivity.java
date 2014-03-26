/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.activities;

import java.util.Locale;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.R;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.fxa.FxAccountAgeLockoutHelper;
import org.mozilla.gecko.fxa.FirefoxAccounts;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.setup.activities.ActivityUtils;

import android.accounts.AccountAuthenticatorActivity;
import android.content.Intent;
import android.os.Bundle;
import android.os.SystemClock;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.TextView;

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

    linkifyOldFirefoxLink();

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

  @Override
  public void onResume() {
    super.onResume();

    Intent intent = null;
    if (FxAccountAgeLockoutHelper.isLockedOut(SystemClock.elapsedRealtime())) {
      intent = new Intent(this, FxAccountCreateAccountNotAllowedActivity.class);
    } else if (FirefoxAccounts.firefoxAccountsExist(this)) {
      intent = new Intent(this, FxAccountStatusActivity.class);
    }

    if (intent != null) {
      this.setAccountAuthenticatorResult(null);
      setResult(RESULT_CANCELED);
      // Per http://stackoverflow.com/a/8992365, this triggers a known bug with
      // the soft keyboard not being shown for the started activity. Why, Android, why?
      intent.setFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
      this.startActivity(intent);
      this.finish();
    }
  }

  /**
   * We started the CreateAccount activity for a result; this returns it to the
   * authenticator.
   */
  @Override
  public void onActivityResult(int requestCode, int resultCode, Intent data) {
    Logger.debug(LOG_TAG, "onActivityResult: " + requestCode + ", " + resultCode);
    if (requestCode != CHILD_REQUEST_CODE) {
      super.onActivityResult(requestCode, resultCode, data);
      return;
    }

    this.setResult(requestCode, data);
    if (data != null) {
      this.setAccountAuthenticatorResult(data.getExtras());

      // We want to drop ourselves off the back stack if the user successfully
      // created or signed in to an account. We can easily determine this by
      // checking for the presence of response data.
      this.finish();
    }
  }

  protected void linkifyOldFirefoxLink() {
    TextView oldFirefox = (TextView) findViewById(R.id.old_firefox);
    String text = getResources().getString(R.string.fxaccount_getting_started_old_firefox);
    String VERSION = AppConstants.MOZ_APP_VERSION;
    String OS = AppConstants.OS_TARGET;
    // We'll need to adjust this when we have active locale switching.
    String LOCALE = Utils.getLanguageTag(Locale.getDefault());
    String url = getResources().getString(R.string.fxaccount_link_old_firefox, VERSION, OS, LOCALE);
    FxAccountConstants.pii(LOG_TAG, "Old Firefox url is: " + url); // Don't want to leak locale in particular.
    ActivityUtils.linkTextView(oldFirefox, text, url);
  }
}
