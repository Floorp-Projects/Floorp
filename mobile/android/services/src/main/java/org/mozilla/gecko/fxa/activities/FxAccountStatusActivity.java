/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.activities;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.accounts.AccountManagerCallback;
import android.accounts.AccountManagerFuture;
import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.app.ActionBar;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.util.TypedValue;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.widget.Toast;
import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.Locales.LocaleAwareFragmentActivity;
import org.mozilla.gecko.R;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.fxa.FxAccountUtils;
import org.mozilla.gecko.fxa.FirefoxAccounts;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.sync.Utils;

/**
 * Activity which displays account status.
 */
public class FxAccountStatusActivity extends LocaleAwareFragmentActivity {
  private static final String LOG_TAG = FxAccountStatusActivity.class.getSimpleName();

  protected FxAccountStatusFragment statusFragment;

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    // Display the fragment as the content.
    statusFragment = new FxAccountStatusFragment();
    getSupportFragmentManager()
      .beginTransaction()
      .replace(android.R.id.content, statusFragment)
      .commit();

    maybeSetHomeButtonEnabled();
  }

  /**
   * Sufficiently recent Android versions need additional code to receive taps
   * on the status bar to go "up". See <a
   * href="http://stackoverflow.com/a/8953148">this stackoverflow answer</a> for
   * more information.
   */
  @TargetApi(Build.VERSION_CODES.ICE_CREAM_SANDWICH)
  protected void maybeSetHomeButtonEnabled() {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.ICE_CREAM_SANDWICH) {
      Logger.debug(LOG_TAG, "Not enabling home button; version too low.");
      return;
    }
    final ActionBar actionBar = getActionBar();
    if (actionBar != null) {
      Logger.debug(LOG_TAG, "Enabling home button.");
      actionBar.setHomeButtonEnabled(true);
      return;
    }
    Logger.debug(LOG_TAG, "Not enabling home button.");
  }

  @Override
  public void onResume() {
    super.onResume();

    final AndroidFxAccount fxAccount = getAndroidFxAccount();
    if (fxAccount == null) {
      Logger.warn(LOG_TAG, "Could not get Firefox Account.");

      // Gracefully redirect to get started.
      final Intent intent = new Intent(FxAccountConstants.ACTION_FXA_GET_STARTED);
      // Per http://stackoverflow.com/a/8992365, this triggers a known bug with
      // the soft keyboard not being shown for the started activity. Why, Android, why?
      intent.setFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
      startActivity(intent);

      setResult(RESULT_CANCELED);
      finish();
      return;
    }
    statusFragment.refresh(fxAccount);
  }

  /**
   * Helper to fetch (unique) Android Firefox Account if one exists, or return null.
   */
  protected AndroidFxAccount getAndroidFxAccount() {
    Account account = FirefoxAccounts.getFirefoxAccount(this);
    if (account == null) {
      return null;
    }
    return new AndroidFxAccount(this, account);
  }


  /**
   * Helper function to maybe remove the given Android account.
   */
  @SuppressLint("InlinedApi")
  public static void maybeDeleteAndroidAccount(final Activity activity, final Account account, final Intent intent) {
    if (account == null) {
      Logger.warn(LOG_TAG, "Trying to delete null account; ignoring request.");
      return;
    }

    final AccountManagerCallback<Boolean> callback = new AccountManagerCallback<Boolean>() {
      @Override
      public void run(AccountManagerFuture<Boolean> future) {
        Logger.info(LOG_TAG, "Account " + Utils.obfuscateEmail(account.name) + " removed.");
        final String text = activity.getResources().getString(R.string.fxaccount_remove_account_toast, account.name);
        Toast.makeText(activity, text, Toast.LENGTH_LONG).show();
        if (intent != null) {
            intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            activity.startActivity(intent);
        }
        activity.finish();
      }
    };

    /*
     * Get the best dialog icon from the theme on v11+.
     * See http://stackoverflow.com/questions/14910536/android-dialog-theme-makes-icon-too-light/14910945#14910945.
     */
    final int icon;
    if (AppConstants.Versions.feature11Plus) {
      final TypedValue typedValue = new TypedValue();
      activity.getTheme().resolveAttribute(android.R.attr.alertDialogIcon, typedValue, true);
      icon = typedValue.resourceId;
    } else {
      icon = android.R.drawable.ic_dialog_alert;
    }

    final AlertDialog dialog = new AlertDialog.Builder(activity)
      .setTitle(R.string.fxaccount_remove_account_dialog_title)
      .setIcon(icon)
      .setMessage(R.string.fxaccount_remove_account_dialog_message)
      .setPositiveButton(android.R.string.ok, new Dialog.OnClickListener() {
        @Override
        public void onClick(DialogInterface dialog, int which) {
          AccountManager.get(activity).removeAccount(account, callback, null);
        }
      })
      .setNegativeButton(android.R.string.cancel, new Dialog.OnClickListener() {
        @Override
        public void onClick(DialogInterface dialog, int which) {
          dialog.cancel();
        }
      })
      .create();

    dialog.show();
  }

  @Override
  public boolean onOptionsItemSelected(MenuItem item) {
    int itemId = item.getItemId();
    if (itemId == android.R.id.home) {
      finish();
      return true;
    }

    if (itemId == R.id.enable_debug_mode) {
      FxAccountUtils.LOG_PERSONAL_INFORMATION = !FxAccountUtils.LOG_PERSONAL_INFORMATION;
      Toast.makeText(this, (FxAccountUtils.LOG_PERSONAL_INFORMATION ? "Enabled" : "Disabled") +
          " Firefox Account personal information!", Toast.LENGTH_LONG).show();
      item.setChecked(!item.isChecked());
      // Display or hide debug options.
      statusFragment.hardRefresh();
      return true;
    }

    return super.onOptionsItemSelected(item);
  }

  @Override
  public boolean onCreateOptionsMenu(Menu menu) {
    final MenuInflater inflater = getMenuInflater();
    inflater.inflate(R.menu.fxaccount_status_menu, menu);
    // !defined(MOZILLA_OFFICIAL) || defined(NIGHTLY_BUILD) || defined(MOZ_DEBUG)
    boolean enabled = !AppConstants.MOZILLA_OFFICIAL || AppConstants.NIGHTLY_BUILD || AppConstants.DEBUG_BUILD;
    if (!enabled) {
      menu.removeItem(R.id.enable_debug_mode);
    } else {
      final MenuItem debugModeItem = menu.findItem(R.id.enable_debug_mode);
      if (debugModeItem != null) {
        // Update checked state based on internal flag.
        menu.findItem(R.id.enable_debug_mode).setChecked(FxAccountUtils.LOG_PERSONAL_INFORMATION);
      }
    }
    return super.onCreateOptionsMenu(menu);
  };
}
