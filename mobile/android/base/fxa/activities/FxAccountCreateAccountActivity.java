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
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.tasks.FxAccountCreateAccountTask;

import android.app.AlertDialog;
import android.app.Dialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.os.SystemClock;
import android.text.Spannable;
import android.text.Spanned;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.AutoCompleteTextView;
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

    emailEdit = (AutoCompleteTextView) ensureFindViewById(null, R.id.email, "email edit");
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
    linkifyPolicy();
    createChooseCheckBox();

    View signInInsteadLink = ensureFindViewById(null, R.id.sign_in_instead_link, "sign in instead link");
    signInInsteadLink.setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View v) {
        final Bundle extras = makeExtrasBundle(null, null);
        startActivityInstead(FxAccountSignInActivity.class, CHILD_REQUEST_CODE, extras);
      }
    });

    updateFromIntentExtras();
  }

  @Override
  protected Bundle makeExtrasBundle(String email, String password) {
    final Bundle extras = super.makeExtrasBundle(email, password);
    final String year = yearEdit.getText().toString();
    extras.putString(EXTRA_YEAR, year);
    return extras;
  }

  @Override
  protected void updateFromIntentExtras() {
    super.updateFromIntentExtras();

    if (getIntent() != null) {
      yearEdit.setText(getIntent().getStringExtra(EXTRA_YEAR));
    }
  }

  @Override
  protected void showClientRemoteException(final FxAccountClientRemoteException e) {
    if (!e.isAccountAlreadyExists()) {
      super.showClientRemoteException(e);
      return;
    }

    // This horrible bit of special-casing is because we want this error message to
    // contain a clickable, extra chunk of text, but we don't want to pollute
    // the exception class with Android specifics.
    final String clickablePart = getString(R.string.fxaccount_sign_in_button_label);
    final String message = getString(e.getErrorMessageStringResource(), clickablePart);
    final int clickableStart = message.lastIndexOf(clickablePart);
    final int clickableEnd = clickableStart + clickablePart.length();

    final Spannable span = Spannable.Factory.getInstance().newSpannable(message);
    span.setSpan(new ClickableSpan() {
      @Override
      public void onClick(View widget) {
        // Pass through the email address that already existed.
        String email = e.body.getString("email");
        if (email == null) {
            email = emailEdit.getText().toString();
        }
        final String password = passwordEdit.getText().toString();

        final Bundle extras = makeExtrasBundle(email, password);
        startActivityInstead(FxAccountSignInActivity.class, CHILD_REQUEST_CODE, extras);
      }
    }, clickableStart, clickableEnd, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
    remoteErrorTextView.setMovementMethod(LinkMovementMethod.getInstance());
    remoteErrorTextView.setText(span);
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

  /**
   * Return years to display in picker.
   *
   * @return 1990 or earlier, 1991, 1992, up to five years before current year.
   *         (So, if it is currently 2014, up to 2009.)
   */
  protected String[] getYearItems() {
    int year = Calendar.getInstance().get(Calendar.YEAR);
    LinkedList<String> years = new LinkedList<String>();
    years.add(getResources().getString(R.string.fxaccount_create_account_1990_or_earlier));
    for (int i = 1991; i <= year - 5; i++) {
      years.add(Integer.toString(i));
    }
    return years.toArray(new String[years.size()]);
  }

  protected void createYearEdit() {
    yearItems = getYearItems();

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
        .setIcon(R.drawable.icon)
        .create();

        dialog.show();
      }
    });
  }

  public void createAccount(String email, String password, Map<String, Boolean> engines) {
    String serverURI = getAuthServerEndpoint();
    PasswordStretcher passwordStretcher = makePasswordStretcher(password);
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
