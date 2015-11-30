/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.activities;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.R;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.fxa.FxAccountClient10.RequestDelegate;
import org.mozilla.gecko.background.fxa.FxAccountClient20.LoginResponse;
import org.mozilla.gecko.background.fxa.FxAccountClientException.FxAccountClientRemoteException;
import org.mozilla.gecko.background.fxa.FxAccountUtils;
import org.mozilla.gecko.background.fxa.PasswordStretcher;
import org.mozilla.gecko.background.fxa.QuickPasswordStretcher;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.fxa.login.Engaged;
import org.mozilla.gecko.fxa.login.State;
import org.mozilla.gecko.fxa.tasks.FxAccountSetupTask.ProgressDisplay;
import org.mozilla.gecko.fxa.tasks.FxAccountUnlockCodeResender;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.SyncConfiguration;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.setup.Constants;
import org.mozilla.gecko.sync.setup.activities.ActivityUtils;
import org.mozilla.gecko.util.ColorUtils;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.animation.LayoutTransition;
import android.annotation.SuppressLint;
import android.content.Context;
import android.content.Intent;
import android.os.AsyncTask;
import android.os.Bundle;
import android.text.Editable;
import android.text.Spannable;
import android.text.TextWatcher;
import android.text.method.LinkMovementMethod;
import android.text.method.PasswordTransformationMethod;
import android.text.method.SingleLineTransformationMethod;
import android.text.style.ClickableSpan;
import android.util.Patterns;
import android.view.KeyEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnFocusChangeListener;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.AutoCompleteTextView;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.TextView.OnEditorActionListener;

abstract public class FxAccountAbstractSetupActivity extends FxAccountAbstractActivity implements ProgressDisplay {
  public static final String EXTRA_EMAIL = "email";
  public static final String EXTRA_PASSWORD = "password";
  public static final String EXTRA_PASSWORD_SHOWN = "password_shown";
  public static final String EXTRA_YEAR = "year";
  public static final String EXTRA_MONTH = "month";
  public static final String EXTRA_DAY = "day";
  public static final String EXTRA_EXTRAS = "extras";

  public static final String JSON_KEY_AUTH = "auth";
  public static final String JSON_KEY_SERVICES = "services";
  public static final String JSON_KEY_SYNC = "sync";
  public static final String JSON_KEY_PROFILE = "profile";

  public FxAccountAbstractSetupActivity() {
    super(CANNOT_RESUME_WHEN_ACCOUNTS_EXIST);
  }

  protected FxAccountAbstractSetupActivity(int resume) {
    super(resume);
  }

  private static final String LOG_TAG = FxAccountAbstractSetupActivity.class.getSimpleName();
}
