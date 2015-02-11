/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.reading;

import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;

import android.accounts.Account;
import android.content.AbstractThreadedSyncAdapter;
import android.content.ContentProviderClient;
import android.content.Context;
import android.content.SyncResult;
import android.os.Bundle;

public class ReadingListSyncAdapter extends AbstractThreadedSyncAdapter {
    public ReadingListSyncAdapter(Context context, boolean autoInitialize) {
      super(context, autoInitialize);
    }

    @Override
    public void onPerformSync(Account account, Bundle extras, String authority, ContentProviderClient provider, SyncResult syncResult) {
      final AndroidFxAccount fxAccount = new AndroidFxAccount(getContext(), account);
    }
}
