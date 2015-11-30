/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.activities;

abstract public class FxAccountAbstractSetupActivity extends FxAccountAbstractActivity {
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
