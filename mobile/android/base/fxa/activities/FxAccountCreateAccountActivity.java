/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.activities;

import java.util.Calendar;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.Locale;
import java.util.Map;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.R;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.fxa.FxAccountAgeLockoutHelper;
import org.mozilla.gecko.background.fxa.FxAccountClient;
import org.mozilla.gecko.background.fxa.FxAccountClient10.RequestDelegate;
import org.mozilla.gecko.background.fxa.FxAccountClient20;
import org.mozilla.gecko.background.fxa.FxAccountClient20.LoginResponse;
import org.mozilla.gecko.background.fxa.FxAccountClientException.FxAccountClientRemoteException;
import org.mozilla.gecko.background.fxa.FxAccountUtils;
import org.mozilla.gecko.background.fxa.PasswordStretcher;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.fxa.tasks.FxAccountCreateAccountTask;
import org.mozilla.gecko.sync.Utils;

import android.app.AlertDialog;
import android.app.Dialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.os.SystemClock;
import android.text.Spannable;
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
  protected String[] monthItems;
  protected String[] dayItems;
  protected EditText yearEdit;
  protected EditText monthEdit;
  protected EditText dayEdit;
  protected CheckBox chooseCheckBox;
  protected View monthDaycombo;

  protected Map<String, Boolean> selectedEngines;
  protected final Map<String, Boolean> authoritiesToSyncAutomaticallyMap =
      new HashMap<String, Boolean>(AndroidFxAccount.DEFAULT_AUTHORITIES_TO_SYNC_AUTOMATICALLY_MAP);

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
    monthEdit = (EditText) ensureFindViewById(null, R.id.month_edit, "month edit");
    dayEdit = (EditText) ensureFindViewById(null, R.id.day_edit, "day edit");
    monthDaycombo = ensureFindViewById(null, R.id.month_day_combo, "month day combo");
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
    initializeMonthAndDayValues();

    View signInInsteadLink = ensureFindViewById(null, R.id.sign_in_instead_link, "sign in instead link");
    signInInsteadLink.setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View v) {
        final Bundle extras = makeExtrasBundle(null, null);
        startActivityInstead(FxAccountSignInActivity.class, CHILD_REQUEST_CODE, extras);
      }
    });

    updateFromIntentExtras();
    maybeEnableAnimations();
  }

  @Override
  protected void onRestoreInstanceState(Bundle savedInstanceState) {
    super.onRestoreInstanceState(savedInstanceState);
    updateMonthAndDayFromBundle(savedInstanceState);
  }

  @Override
  protected void onSaveInstanceState(Bundle outState) {
    super.onSaveInstanceState(outState);
    updateBundleWithMonthAndDay(outState);
  }

  @Override
  protected Bundle makeExtrasBundle(String email, String password) {
    final Bundle extras = super.makeExtrasBundle(email, password);
    extras.putString(EXTRA_YEAR, yearEdit.getText().toString());
    updateBundleWithMonthAndDay(extras);
    return extras;
  }

  @Override
  protected void updateFromIntentExtras() {
    super.updateFromIntentExtras();

    if (getIntent() != null) {
      yearEdit.setText(getIntent().getStringExtra(EXTRA_YEAR));
      updateMonthAndDayFromBundle(getIntent().getExtras() != null ? getIntent().getExtras() : new Bundle());
    }
  }

  private void updateBundleWithMonthAndDay(final Bundle bundle) {
    if (monthEdit.getTag() != null) {
      bundle.putInt(EXTRA_MONTH, (Integer) monthEdit.getTag());
    }
    if (dayEdit.getTag() != null) {
      bundle.putInt(EXTRA_DAY, (Integer) dayEdit.getTag());
    }
  }

  private void updateMonthAndDayFromBundle(final Bundle extras) {
    final Integer zeroBasedMonthIndex = (Integer) extras.get(EXTRA_MONTH);
    final Integer oneBasedDayIndex = (Integer) extras.get(EXTRA_DAY);
    maybeEnableMonthAndDayButtons();

    if (zeroBasedMonthIndex != null) {
      monthEdit.setText(monthItems[zeroBasedMonthIndex]);
      monthEdit.setTag(Integer.valueOf(zeroBasedMonthIndex));
      createDayEdit(zeroBasedMonthIndex);

      if (oneBasedDayIndex != null && dayItems != null) {
        dayEdit.setText(dayItems[oneBasedDayIndex - 1]);
        dayEdit.setTag(Integer.valueOf(oneBasedDayIndex));
      }
    } else {
      monthEdit.setText("");
      dayEdit.setText("");
    }
    updateButtonState();
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
    final int messageId = e.getErrorMessageStringResource();
    final int clickableId = R.string.fxaccount_sign_in_button_label;

    final Spannable span = Utils.interpolateClickableSpan(this, messageId, clickableId, new ClickableSpan() {
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
    });
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
            maybeEnableMonthAndDayButtons();
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

  private void initializeMonthAndDayValues() {
    // Hide Month and day pickers
    monthDaycombo.setVisibility(View.GONE);
    dayEdit.setEnabled(false);

    // Populate month names.
    final Calendar calendar = Calendar.getInstance();
    final Map<String, Integer> monthNamesMap = calendar.getDisplayNames(Calendar.MONTH, Calendar.LONG, Locale.getDefault());
    monthItems = new String[monthNamesMap.size()];
    for (Map.Entry<String, Integer> entry : monthNamesMap.entrySet()) {
      monthItems[entry.getValue()] = entry.getKey();
    }
    createMonthEdit();
  }

  protected void createMonthEdit() {
    monthEdit.setText("");
    monthEdit.setTag(null);
    monthEdit.setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View v) {
        android.content.DialogInterface.OnClickListener listener = new Dialog.OnClickListener() {
	      @Override
          public void onClick(DialogInterface dialog, int which) {
            monthEdit.setText(monthItems[which]);
            monthEdit.setTag(Integer.valueOf(which));
            createDayEdit(which);
            updateButtonState();
          }
        };
        final AlertDialog dialog = new AlertDialog.Builder(FxAccountCreateAccountActivity.this)
        .setTitle(R.string.fxaccount_create_account_month_of_birth)
        .setItems(monthItems, listener)
        .setIcon(R.drawable.icon)
        .create();
        dialog.show();
      }
    });
  }

  protected void createDayEdit(final int monthIndex) {
    dayEdit.setText("");
    dayEdit.setTag(null);
    dayEdit.setEnabled(true);

    String yearText = yearEdit.getText().toString();
    Integer birthYear;
    try {
      birthYear = Integer.parseInt(yearText);
    } catch (NumberFormatException e) {
      // Ideal this should never happen.
      Logger.debug(LOG_TAG, "Exception while parsing year value" + e);
      return;
    }

    Calendar c = Calendar.getInstance();
    c.set(birthYear, monthIndex, 1);
    LinkedList<String> days = new LinkedList<String>();
    for (int i = c.getActualMinimum(Calendar.DAY_OF_MONTH); i <= c.getActualMaximum(Calendar.DAY_OF_MONTH); i++) {
      days.add(Integer.toString(i));
    }
    dayItems = days.toArray(new String[days.size()]);

    dayEdit.setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View v) {
        android.content.DialogInterface.OnClickListener listener = new Dialog.OnClickListener() {
	      @Override
          public void onClick(DialogInterface dialog, int which) {
            dayEdit.setText(dayItems[which]);
            dayEdit.setTag(Integer.valueOf(which + 1)); // Days are 1-based.
            updateButtonState();
          }
        };
        final AlertDialog dialog = new AlertDialog.Builder(FxAccountCreateAccountActivity.this)
        .setTitle(R.string.fxaccount_create_account_day_of_birth)
        .setItems(dayItems, listener)
        .setIcon(R.drawable.icon)
        .create();
        dialog.show();
      }
    });
  }

  private void maybeEnableMonthAndDayButtons() {
    Integer yearOfBirth = null;
    try {
      yearOfBirth = Integer.valueOf(yearEdit.getText().toString(), 10);
    } catch (NumberFormatException e) {
      Logger.debug(LOG_TAG, "Year text is not a number; assuming year is a range and that user is old enough.");
    }

    // Check if the selected year is the magic year.
    if (yearOfBirth == null || !FxAccountAgeLockoutHelper.isMagicYear(yearOfBirth)) {
      // Year/Dec/31 is the latest birthday in the selected year, corresponding
      // to the youngest person.
      monthEdit.setTag(Integer.valueOf(11));
      dayEdit.setTag(Integer.valueOf(31));
      return;
    }

    // Show month and date field.
    yearEdit.setVisibility(View.GONE);
    monthDaycombo.setVisibility(View.VISIBLE);
    monthEdit.setTag(null);
    dayEdit.setTag(null);
  }

  public void createAccount(String email, String password, Map<String, Boolean> engines, Map<String, Boolean> authoritiesToSyncAutomaticallyMap) {
    String serverURI = getAuthServerEndpoint();
    PasswordStretcher passwordStretcher = makePasswordStretcher(password);
    // This delegate creates a new Android account on success, opens the
    // appropriate "success!" activity, and finishes this activity.
    RequestDelegate<LoginResponse> delegate = new AddAccountDelegate(email, passwordStretcher, serverURI, engines, authoritiesToSyncAutomaticallyMap) {
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
      new FxAccountCreateAccountTask(this, this, email, passwordStretcher, client, getQueryParameters(), delegate).execute();
    } catch (Exception e) {
      showRemoteError(e, R.string.fxaccount_create_account_unknown_error);
    }
  }

  @Override
  protected boolean shouldButtonBeEnabled() {
    return super.shouldButtonBeEnabled() &&
        (yearEdit.length() > 0) &&
        (monthEdit.getTag() != null) &&
        (dayEdit.getTag() != null);
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
        final int dayOfBirth = (Integer) dayEdit.getTag();
        final int zeroBasedMonthOfBirth = (Integer) monthEdit.getTag();
        // Only include selected engines if the user currently has the option checked.
        final Map<String, Boolean> engines = chooseCheckBox.isChecked()
            ? selectedEngines
            : null;
        // Only include authorities if the user currently has the option checked.
        final Map<String, Boolean> authoritiesMap = chooseCheckBox.isChecked()
            ? authoritiesToSyncAutomaticallyMap
            : AndroidFxAccount.DEFAULT_AUTHORITIES_TO_SYNC_AUTOMATICALLY_MAP;
        if (FxAccountAgeLockoutHelper.passesAgeCheck(dayOfBirth, zeroBasedMonthOfBirth, yearEdit.getText().toString(), yearItems)) {
          FxAccountUtils.pii(LOG_TAG, "Passed age check.");
          createAccount(email, password, engines, authoritiesMap);
        } else {
          FxAccountUtils.pii(LOG_TAG, "Failed age check!");
          FxAccountAgeLockoutHelper.lockOut(SystemClock.elapsedRealtime());
          setResult(RESULT_CANCELED);
          launchActivity(FxAccountCreateAccountNotAllowedActivity.class);
          finish();
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
    final int INDEX_READING_LIST = 4; // Only valid if reading list is enabled.
    final int NUMBER_OF_ENGINES;
    if (AppConstants.MOZ_ANDROID_READING_LIST_SERVICE) {
      NUMBER_OF_ENGINES = 5;
    } else {
      NUMBER_OF_ENGINES = 4;
    }

    final String items[] = new String[NUMBER_OF_ENGINES];
    final boolean checkedItems[] = new boolean[NUMBER_OF_ENGINES];
    items[INDEX_BOOKMARKS] = getResources().getString(R.string.fxaccount_status_bookmarks);
    items[INDEX_HISTORY] = getResources().getString(R.string.fxaccount_status_history);
    items[INDEX_TABS] = getResources().getString(R.string.fxaccount_status_tabs);
    items[INDEX_PASSWORDS] = getResources().getString(R.string.fxaccount_status_passwords);
    if (AppConstants.MOZ_ANDROID_READING_LIST_SERVICE) {
      items[INDEX_READING_LIST] = getResources().getString(R.string.fxaccount_status_reading_list);
    }
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
        if (AppConstants.MOZ_ANDROID_READING_LIST_SERVICE) {
          authoritiesToSyncAutomaticallyMap.put(BrowserContract.READING_LIST_AUTHORITY, checkedItems[INDEX_READING_LIST]);
        }
        FxAccountUtils.pii(LOG_TAG, "Updating selectedEngines: " + selectedEngines.toString());
        FxAccountUtils.pii(LOG_TAG, "Updating authorities: " + authoritiesToSyncAutomaticallyMap.toString());
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
