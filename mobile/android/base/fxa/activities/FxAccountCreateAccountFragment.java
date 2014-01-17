/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.activities;

import android.support.v4.app.Fragment;

public class FxAccountCreateAccountFragment extends Fragment { // implements OnClickListener {
  protected static final String LOG_TAG = FxAccountCreateAccountFragment.class.getSimpleName();
//
//  protected FxAccountSetupActivity activity;
//
//  protected EditText emailEdit;
//  protected EditText passwordEdit;
//  protected EditText password2Edit;
//  protected Button button;
//
//  protected TextView emailError;
//  protected TextView passwordError;
//
//  protected TextChangedListener textChangedListener;
//  protected EditorActionListener editorActionListener;
//  protected OnFocusChangeListener focusChangeListener;
//  @Override
//  public void onCreate(Bundle savedInstanceState) {
//    super.onCreate(savedInstanceState);
//    // Retain this fragment across configuration changes. See, for example,
//    // http://www.androiddesignpatterns.com/2013/04/retaining-objects-across-config-changes.html
//    // This fragment will own AsyncTask instances which should not be
//    // interrupted by configuration changes (and activity changes).
//    setRetainInstance(true);
//  }

//  @Override
//  public View onCreateView(LayoutInflater inflater, ViewGroup container,
//      Bundle savedInstanceState) {
//    View v = inflater.inflate(R.layout.fxaccount_create_account_fragment, container, false);
//
//    FxAccountSetupActivity.linkifyTextViews(v, new int[] { R.id.description, R.id.policy });
//
//    emailEdit = (EditText) ensureFindViewById(v, R.id.email, "email");
//    passwordEdit = (EditText) ensureFindViewById(v, R.id.password, "password");
//    // Second password can be null.
//    password2Edit = (EditText) v.findViewById(R.id.password2);
//
//    emailError = (TextView) ensureFindViewById(v, R.id.email_error, "email error");
//    passwordError = (TextView) ensureFindViewById(v, R.id.password_error, "password error");
//
//    textChangedListener = new TextChangedListener();
//    editorActionListener = new EditorActionListener();
//    focusChangeListener = new FocusChangeListener();
//
//    addListeners(emailEdit);
//    addListeners(passwordEdit);
//    if (password2Edit != null) {
//      addListeners(password2Edit);
//    }
//
//    button = (Button) ensureFindViewById(v, R.id.create_account_button, "button");
//    button.setOnClickListener(this);
//    return v;
//  }

//  protected void onCreateAccount(View button) {
//    Logger.debug(LOG_TAG, "onCreateAccount: Asking for username/password for new account.");
//    String email = emailEdit.getText().toString();
//    String password = passwordEdit.getText().toString();
//    activity.signUp(email, password);
//  }
//
//  @Override
//  public void onClick(View v) {
//    switch (v.getId()) {
//    case R.id.create_account_button:
//      if (!validate(false)) {
//        return;
//      }
//      onCreateAccount(v);
//      break;
//    }
//  }
//
//  protected void addListeners(EditText editText) {
//    editText.addTextChangedListener(textChangedListener);
//    editText.setOnEditorActionListener(editorActionListener);
//    editText.setOnFocusChangeListener(focusChangeListener);
//  }
//
//  protected class FocusChangeListener implements OnFocusChangeListener {
//    @Override
//    public void onFocusChange(View v, boolean hasFocus) {
//      if (hasFocus) {
//        return;
//      }
//      validate(false);
//    }
//  }
//
//  protected class EditorActionListener implements OnEditorActionListener {
//    @Override
//    public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
//      validate(false);
//      return false;
//    }
//  }
//
//  protected class TextChangedListener implements TextWatcher {
//    @Override
//    public void afterTextChanged(Editable s) {
//      validate(true);
//    }
//
//    @Override
//    public void beforeTextChanged(CharSequence s, int start, int count, int after) {
//      // Do nothing.
//    }
//
//    @Override
//    public void onTextChanged(CharSequence s, int start, int before, int count) {
//      // Do nothing.
//    }
//  }
//
//  /**
//   * Show or hide error messaging.
//   *
//   * @param removeOnly
//   *          if true, possibly remove existing error messages but do not set an
//   *          error message if one was not present.
//   * @param errorResourceId
//   *          of error string, or -1 to hide.
//   * @param errorView
//   *          <code>TextView</code> instance to display error message in.
//   * @param edits
//   *          <code>EditText</code> instances to style.
//   */
//  protected void setError(boolean removeOnly, int errorResourceId, TextView errorView, EditText... edits) {
//    if (removeOnly && errorResourceId != -1) {
//      return;
//    }
//
//    int res = errorResourceId == -1 ? R.drawable.fxaccount_textfield_background : R.drawable.fxaccount_textfield_error_background;
//    for (EditText edit : edits) {
//      if (edit == null) {
//        continue;
//      }
//      edit.setBackgroundResource(res);
//    }
//    if (errorResourceId == -1) {
//      errorView.setVisibility(View.GONE);
//      errorView.setText(null);
//    } else {
//      errorView.setText(errorResourceId);
//      errorView.setVisibility(View.VISIBLE);
//    }
//  }
//
//  protected boolean validate(boolean removeOnly) {
//    boolean enabled = true;
//    final String email = emailEdit.getText().toString();
//    final String password = passwordEdit.getText().toString();
//
//    if (email.length() == 0 || Patterns.EMAIL_ADDRESS.matcher(email).matches()) {
//      setError(removeOnly, -1, emailError, emailEdit);
//    } else {
//      enabled = false;
//      setError(removeOnly, R.string.fxaccount_bad_email, emailError, emailEdit);
//    }
//
//    if (password2Edit != null) {
//      final String password2 = password2Edit.getText().toString();
//      enabled = enabled && password2.length() > 0;
//
//      boolean passwordsMatch = password.equals(password2);
//      if (passwordsMatch) {
//        setError(removeOnly, -1, passwordError, passwordEdit, password2Edit);
//      } else {
//        enabled = false;
//        setError(removeOnly, R.string.fxaccount_bad_passwords, passwordError, passwordEdit, password2Edit);
//      }
//    }
//
//    if (enabled != button.isEnabled()) {
//      Logger.debug(LOG_TAG, (enabled ? "En" : "Dis") + "abling button.");
//      button.setEnabled(enabled);
//    }
//
//    return enabled;
//  }
}
