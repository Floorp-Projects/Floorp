/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.activities;

import java.io.IOException;

import org.mozilla.gecko.R;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.fxa.FxAccountClientException.FxAccountClientRemoteException;
import org.mozilla.gecko.fxa.activities.FxAccountSetupTask.ProgressDisplay;

import android.text.Editable;
import android.text.InputType;
import android.text.TextWatcher;
import android.util.Patterns;
import android.view.KeyEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnFocusChangeListener;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.TextView.OnEditorActionListener;

abstract public class FxAccountAbstractSetupActivity extends FxAccountAbstractActivity implements ProgressDisplay {
  public FxAccountAbstractSetupActivity() {
    super(CANNOT_RESUME_WHEN_ACCOUNTS_EXIST | CANNOT_RESUME_WHEN_LOCKED_OUT);
  }

  protected FxAccountAbstractSetupActivity(int resume) {
    super(resume);
  }

  private static final String LOG_TAG = FxAccountAbstractSetupActivity.class.getSimpleName();

  protected int minimumPasswordLength = 8;

  protected EditText emailEdit;
  protected EditText passwordEdit;
  protected Button showPasswordButton;
  protected TextView remoteErrorTextView;
  protected Button button;
  protected ProgressBar progressBar;

  protected void createShowPasswordButton() {
    showPasswordButton.setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View v) {
        boolean isShown = 0 == (passwordEdit.getInputType() & InputType.TYPE_TEXT_VARIATION_PASSWORD);
        // Changing input type loses position in edit text; let's try to maintain it.
        int start = passwordEdit.getSelectionStart();
        int stop = passwordEdit.getSelectionEnd();
        passwordEdit.setInputType(passwordEdit.getInputType() ^ InputType.TYPE_TEXT_VARIATION_PASSWORD);
        passwordEdit.setSelection(start, stop);
        if (isShown) {
          showPasswordButton.setText(R.string.fxaccount_password_show);
        } else {
          showPasswordButton.setText(R.string.fxaccount_password_hide);
        }
      }
    });
  }

  protected void hideRemoteError() {
    remoteErrorTextView.setVisibility(View.INVISIBLE);
  }

  protected void showRemoteError(Exception e, int defaultResourceId) {
    if (e instanceof IOException) {
      remoteErrorTextView.setText(R.string.fxaccount_remote_error_COULD_NOT_CONNECT);
    } else if (e instanceof FxAccountClientRemoteException) {
      remoteErrorTextView.setText(((FxAccountClientRemoteException) e).getErrorMessageStringResource());
    } else {
      remoteErrorTextView.setText(defaultResourceId);
    }
    Logger.warn(LOG_TAG, "Got exception; showing error message: " + remoteErrorTextView.getText().toString(), e);
    remoteErrorTextView.setVisibility(View.VISIBLE);
  }

  protected void addListeners() {
    TextChangedListener textChangedListener = new TextChangedListener();
    EditorActionListener editorActionListener = new EditorActionListener();
    FocusChangeListener focusChangeListener = new FocusChangeListener();

    emailEdit.addTextChangedListener(textChangedListener);
    emailEdit.setOnEditorActionListener(editorActionListener);
    emailEdit.setOnFocusChangeListener(focusChangeListener);
    passwordEdit.addTextChangedListener(textChangedListener);
    passwordEdit.setOnEditorActionListener(editorActionListener);
    passwordEdit.setOnFocusChangeListener(focusChangeListener);
  }

  protected class FocusChangeListener implements OnFocusChangeListener {
    @Override
    public void onFocusChange(View v, boolean hasFocus) {
      if (hasFocus) {
        return;
      }
      updateButtonState();
    }
  }

  protected class EditorActionListener implements OnEditorActionListener {
    @Override
    public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
      updateButtonState();
      return false;
    }
  }

  protected class TextChangedListener implements TextWatcher {
    @Override
    public void afterTextChanged(Editable s) {
      updateButtonState();
    }

    @Override
    public void beforeTextChanged(CharSequence s, int start, int count, int after) {
      // Do nothing.
    }

    @Override
    public void onTextChanged(CharSequence s, int start, int before, int count) {
      // Do nothing.
    }
  }

  protected boolean shouldButtonBeEnabled() {
    final String email = emailEdit.getText().toString();
    final String password = passwordEdit.getText().toString();

    boolean enabled =
        (email.length() > 0) &&
        Patterns.EMAIL_ADDRESS.matcher(email).matches() &&
        (password.length() >= minimumPasswordLength);
    return enabled;
  }

  protected boolean updateButtonState() {
    boolean enabled = shouldButtonBeEnabled();
    if (!enabled) {
      // The user needs to do something before you can interact with the button;
      // presumably that interaction will fix whatever error is shown.
      hideRemoteError();
    }
    if (enabled != button.isEnabled()) {
      Logger.debug(LOG_TAG, (enabled ? "En" : "Dis") + "abling button.");
      button.setEnabled(enabled);
    }
    return enabled;
  }

  @Override
  public void showProgress() {
    progressBar.setVisibility(View.VISIBLE);
    button.setVisibility(View.INVISIBLE);
  }

  @Override
  public void dismissProgress() {
    progressBar.setVisibility(View.INVISIBLE);
    button.setVisibility(View.VISIBLE);
  }
}
