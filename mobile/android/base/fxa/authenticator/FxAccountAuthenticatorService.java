/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.authenticator;

import org.mozilla.gecko.background.common.log.Logger;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

public class FxAccountAuthenticatorService extends Service {
  public static final String LOG_TAG = FxAccountAuthenticatorService.class.getSimpleName();

  // Lazily initialized by <code>getAuthenticator</code>.
  protected FxAccountAuthenticator accountAuthenticator = null;

  protected synchronized FxAccountAuthenticator getAuthenticator() {
    if (accountAuthenticator == null) {
      accountAuthenticator = new FxAccountAuthenticator(this);
    }

    return accountAuthenticator;
  }

  @Override
  public void onCreate() {
    Logger.debug(LOG_TAG, "onCreate");

    accountAuthenticator = getAuthenticator();
  }

  @Override
  public IBinder onBind(Intent intent) {
    Logger.debug(LOG_TAG, "onBind");

    if (intent == null) {
      // Should never happen, but can -- Bug 1025937.
      return null;
    }

    if (!android.accounts.AccountManager.ACTION_AUTHENTICATOR_INTENT.equals(intent.getAction())) {
      return null;
    }

    final FxAccountAuthenticator authenticator = getAuthenticator();
    if (authenticator == null) {
      // Should never happen.
      return null;
    }

    return authenticator.getIBinder();
  }
}
