/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.activities;

import java.util.Calendar;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.Map;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;

import org.mozilla.gecko.R;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.fxa.FxAccountAgeLockoutHelper;
import org.mozilla.gecko.background.fxa.FxAccountClient;
import org.mozilla.gecko.background.fxa.FxAccountClient10.RequestDelegate;
import org.mozilla.gecko.background.fxa.FxAccountClient20;
import org.mozilla.gecko.background.fxa.FxAccountClient20.LoginResponse;
import org.mozilla.gecko.background.fxa.FxAccountClientException.FxAccountClientRemoteException;
import org.mozilla.gecko.background.fxa.PasswordStretcher;
import org.mozilla.gecko.background.fxa.QuickPasswordStretcher;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.activities.FxAccountSetupTask.FxAccountCreateAccountTask;
import org.mozilla.gecko.sync.setup.activities.ActivityUtils;

import android.app.AlertDialog;
import android.app.Dialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.os.SystemClock;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.ListView;
import android.widget.ProgressBar;
import android.widget.TextView;

/**
 * Activity which displays create account screen to the user.
 */
public class FxAccountCreateAccountActivity extends FxAccountAbstractSetupActivity {
  protected static final String LOG_TAG = FxAccountCreateAccountActivity.class.getSimpleName();

  private static final int CHILD_REQUEST_CODE = 2;

  protected String[] yearItems;
  protected EditText yearEdit;
  protected CheckBox chooseCheckBox;

  protected Map<String, Boolean> selectedEngines;

  /**
   * {@inheritDoc}
   */
  @Override
  public void onCreate(Bundle icicle) {
    Logger.debug(LOG_TAG, "onCreate(" + icicle + ")");

    super.onCreate(icicle);
    setContentView(R.layout.fxaccount_create_account);

    TextView policyView = (TextView) ensureFindViewById(null, R.id.policy, "policy links");
    final String linkTerms = getString(R.string.fxaccount_link_tos);
    final String linkPrivacy = getString(R.string.fxaccount_link_pn);
    final String linkedTOS = "<a href=\"" + linkTerms + "\">" + getString(R.string.fxaccount_policy_linktos) + "</a>";
    final String linkedPN = "<a href=\"" + linkPrivacy + "\">" + getString(R.string.fxaccount_policy_linkprivacy) + "</a>";
    policyView.setText(getString(R.string.fxaccount_create_account_policy_text, linkedTOS, linkedPN));
    final boolean underlineLinks = true;
    ActivityUtils.linkifyTextView(policyView, underlineLinks);

    emailEdit = (EditText) ensureFindViewById(null, R.id.email, "email edit");
    passwordEdit = (EditText) ensureFindViewById(null, R.id.password, "password edit");
    showPasswordButton = (Button) ensureFindViewById(null, R.id.show_password, "show password button");
    yearEdit = (EditText) ensureFindViewById(null, R.id.year_edit, "year edit");
    remoteErrorTextView = (TextView) ensureFindViewById(null, R.id.remote_error, "remote error text view");
    button = (Button) ensureFindViewById(null, R.id.button, "create account button");
    progressBar = (ProgressBar) ensureFindViewById(null, R.id.progress, "progress bar");
    chooseCheckBox = (CheckBox) ensureFindViewById(null, R.id.choose_what_to_sync_checkbox, "choose what to sync check box");
    selectedEngines = new HashMap<String, Boolean>();

    createCreateAccountButton();
    createYearEdit();
    addListeners();
    updateButtonState();
    createShowPasswordButton();
    createChooseCheckBox();

    View signInInsteadLink = ensureFindViewById(null, R.id.sign_in_instead_link, "sign in instead link");
    signInInsteadLink.setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View v) {
        Intent intent = new Intent(FxAccountCreateAccountActivity.this, FxAccountSignInActivity.class);
        intent.putExtra("email", emailEdit.getText().toString());
        intent.putExtra("password", passwordEdit.getText().toString());
        // Per http://stackoverflow.com/a/8992365, this triggers a known bug with
        // the soft keyboard not being shown for the started activity. Why, Android, why?
        intent.setFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
        startActivityForResult(intent, CHILD_REQUEST_CODE);
      }
    });

    // Only set email/password in onCreate; we don't want to overwrite edited values onResume.
    if (getIntent() != null && getIntent().getExtras() != null) {
      Bundle bundle = getIntent().getExtras();
      emailEdit.setText(bundle.getString("email"));
      passwordEdit.setText(bundle.getString("password"));
    }
  }

  /**
   * We might have switched to the SignIn activity; if that activity
   * succeeds, feed its result back to the authenticator.
   */
  @Override
  public void onActivityResult(int requestCode, int resultCode, Intent data) {
    Logger.debug(LOG_TAG, "onActivityResult: " + requestCode);
    if (requestCode != CHILD_REQUEST_CODE || resultCode != RESULT_OK) {
      super.onActivityResult(requestCode, resultCode, data);
      return;
    }
    this.setResult(resultCode, data);
    this.finish();
  }

  protected void createYearEdit() {
    int year = Calendar.getInstance().get(Calendar.YEAR);
    LinkedList<String> years = new LinkedList<String>();
    for (int i = year - 5; i >= 1951; i--) {
      years.add(Integer.toString(i));
    }
    years.add(getResources().getString(R.string.fxaccount_create_account_1950_or_earlier));
    yearItems = years.toArray(new String[0]);

    yearEdit.setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View v) {
        android.content.DialogInterface.OnClickListener listener = new Dialog.OnClickListener() {
          @Override
          public void onClick(DialogInterface dialog, int which) {
            yearEdit.setText(yearItems[which]);
            updateButtonState();
          }
        };
        final AlertDialog dialog = new AlertDialog.Builder(FxAccountCreateAccountActivity.this)
        .setTitle(R.string.fxaccount_create_account_year_of_birth)
        .setItems(yearItems, listener)
        .setIcon(R.drawable.fxaccount_icon)
        .create();

        dialog.show();
      }
    });
  }

  public void createAccount(String email, String password, Map<String, Boolean> engines) {
    String serverURI = FxAccountConstants.DEFAULT_AUTH_SERVER_ENDPOINT;
    PasswordStretcher passwordStretcher = new QuickPasswordStretcher(password);
    // This delegate creates a new Android account on success, opens the
    // appropriate "success!" activity, and finishes this activity.
    RequestDelegate<LoginResponse> delegate = new AddAccountDelegate(email, passwordStretcher, serverURI, engines) {
      @Override
      public void handleError(Exception e) {
        showRemoteError(e, R.string.fxaccount_create_account_unknown_error);
      }

      @Override
      public void handleFailure(FxAccountClientRemoteException e) {
        showRemoteError(e, R.string.fxaccount_create_account_unknown_error);
      }
    };

    Executor executor = Executors.newSingleThreadExecutor();
    FxAccountClient client = new FxAccountClient20(serverURI, executor);
    try {
      hideRemoteError();
      new FxAccountCreateAccountTask(this, this, email, passwordStretcher, client, delegate).execute();
    } catch (Exception e) {
      showRemoteError(e, R.string.fxaccount_create_account_unknown_error);
    }
  }

  @Override
  protected boolean shouldButtonBeEnabled() {
    return super.shouldButtonBeEnabled() &&
        (yearEdit.length() > 0);
  }

  protected void createCreateAccountButton() {
    button.setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View v) {
        if (!updateButtonState()) {
          return;
        }
        final String email = emailEdit.getText().toString();
        final String password = passwordEdit.getText().toString();
        // Only include selected engines if the user currently has the option checked.
        final Map<String, Boolean> engines = chooseCheckBox.isChecked()
            ? selectedEngines
            : null;
        if (FxAccountAgeLockoutHelper.passesAgeCheck(yearEdit.getText().toString(), yearItems)) {
          FxAccountConstants.pii(LOG_TAG, "Passed age check.");
          createAccount(email, password, engines);
        } else {
          FxAccountConstants.pii(LOG_TAG, "Failed age check!");
          FxAccountAgeLockoutHelper.lockOut(SystemClock.elapsedRealtime());
          setResult(RESULT_CANCELED);
          redirectToActivity(FxAccountCreateAccountNotAllowedActivity.class);
        }
      }
    });
  }

  /**
   * The "Choose what to sync" checkbox pops up a multi-choice dialog when it is
   * unchecked. It toggles to unchecked from checked.
   */
  protected void createChooseCheckBox() {
    final int INDEX_BOOKMARKS = 0;
    final int INDEX_HISTORY = 1;
    final int INDEX_TABS = 2;
    final int INDEX_PASSWORDS = 3;
    final int NUMBER_OF_ENGINES = 4;

    final String items[] = new String[NUMBER_OF_ENGINES];
    final boolean checkedItems[] = new boolean[NUMBER_OF_ENGINES];
    items[INDEX_BOOKMARKS] = getResources().getString(R.string.fxaccount_status_bookmarks);
    items[INDEX_HISTORY] = getResources().getString(R.string.fxaccount_status_history);
    items[INDEX_TABS] = getResources().getString(R.string.fxaccount_status_tabs);
    items[INDEX_PASSWORDS] = getResources().getString(R.string.fxaccount_status_passwords);
    // Default to everything checked.
    for (int i = 0; i < NUMBER_OF_ENGINES; i++) {
      checkedItems[i] = true;
    }

    final DialogInterface.OnClickListener clickListener = new DialogInterface.OnClickListener() {
      @Override
      public void onClick(DialogInterface dialog, int which) {
        if (which != DialogInterface.BUTTON_POSITIVE) {
          Logger.debug(LOG_TAG, "onClick: not button positive, unchecking.");
          chooseCheckBox.setChecked(false);
          return;
        }
        // We only check the box on success.
        Logger.debug(LOG_TAG, "onClick: button positive, checking.");
        chooseCheckBox.setChecked(true);
        // And then remember for future use.
        ListView selectionsList = ((AlertDialog) dialog).getListView();
        for (int i = 0; i < NUMBER_OF_ENGINES; i++) {
          checkedItems[i] = selectionsList.isItemChecked(i);
        }
        selectedEngines.put("bookmarks", checkedItems[INDEX_BOOKMARKS]);
        selectedEngines.put("history", checkedItems[INDEX_HISTORY]);
        selectedEngines.put("tabs", checkedItems[INDEX_TABS]);
        selectedEngines.put("passwords", checkedItems[INDEX_PASSWORDS]);
        FxAccountConstants.pii(LOG_TAG, "Updating selectedEngines: " + selectedEngines.toString());
      }
    };

    final DialogInterface.OnMultiChoiceClickListener multiChoiceClickListener = new DialogInterface.OnMultiChoiceClickListener() {
      @Override
      public void onClick(DialogInterface dialog, int which, boolean isChecked) {
        // Display multi-selection clicks in UI.
        ListView selectionsList = ((AlertDialog) dialog).getListView();
        selectionsList.setItemChecked(which, isChecked);
      }
    };

    final AlertDialog dialog = new AlertDialog.Builder(this)
        .setTitle(R.string.fxaccount_create_account_choose_what_to_sync)
        .setIcon(R.drawable.icon)
        .setMultiChoiceItems(items, checkedItems, multiChoiceClickListener)
        .setPositiveButton(android.R.string.ok, clickListener)
        .setNegativeButton(android.R.string.cancel, clickListener)
        .create();

    dialog.setOnCancelListener(new DialogInterface.OnCancelListener() {
      @Override
      public void onCancel(DialogInterface dialog) {
        Logger.debug(LOG_TAG, "onCancel: unchecking.");
        chooseCheckBox.setChecked(false);
      }
    });

    chooseCheckBox.setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View v) {
        // There appears to be no way to stop Android interpreting the click
        // first. So, if the user clicked on an unchecked box, it's checked by
        // the time we get here.
        if (!chooseCheckBox.isChecked()) {
          Logger.debug(LOG_TAG, "onClick: was checked, not showing dialog.");
          return;
        }
        Logger.debug(LOG_TAG, "onClick: was unchecked, showing dialog.");
        dialog.show();
      }
    });
  }
}
