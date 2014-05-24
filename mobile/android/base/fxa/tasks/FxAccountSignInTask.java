/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.tasks;

import java.io.UnsupportedEncodingException;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.fxa.FxAccountClient;
import org.mozilla.gecko.background.fxa.FxAccountClient10.RequestDelegate;
import org.mozilla.gecko.background.fxa.FxAccountClient20.LoginResponse;
import org.mozilla.gecko.background.fxa.PasswordStretcher;

import android.content.Context;

public class FxAccountSignInTask extends FxAccountSetupTask<LoginResponse> {
  protected static final String LOG_TAG = FxAccountSignInTask.class.getSimpleName();

  protected final byte[] emailUTF8;
  protected final PasswordStretcher passwordStretcher;

  public FxAccountSignInTask(Context context, ProgressDisplay progressDisplay, String email, PasswordStretcher passwordStretcher, FxAccountClient client, RequestDelegate<LoginResponse> delegate) throws UnsupportedEncodingException {
    super(context, progressDisplay, client, delegate);
    this.emailUTF8 = email.getBytes("UTF-8");
    this.passwordStretcher = passwordStretcher;
  }

  @Override
  protected InnerRequestDelegate<LoginResponse> doInBackground(Void... arg0) {
    try {
      client.loginAndGetKeys(emailUTF8, passwordStretcher, innerDelegate);
      latch.await();
      return innerDelegate;
    } catch (Exception e) {
      Logger.error(LOG_TAG, "Got exception signing in.", e);
      delegate.handleError(e);
    }
    return null;
  }
}
