/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.preference.Preference;
import android.util.AttributeSet;
import android.util.Log;

import org.mozilla.gecko.sync.setup.activities.SetupSyncActivity;

class SyncPreference extends Preference {
    private static final String FENNEC_ACCOUNT_TYPE = "org.mozilla.firefox_sync";
    private static final String SYNC_SETTINGS = "android.settings.SYNC_SETTINGS";

    private Context mContext;

    public SyncPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
    }

    @Override
    protected void onClick() {
        // show sync setup if no accounts exist; otherwise, show account settings
        Account[] accounts = AccountManager.get(mContext).getAccountsByType(FENNEC_ACCOUNT_TYPE);
        Intent intent;
        if (accounts.length > 0)
            intent = new Intent(SYNC_SETTINGS);
        else
            intent = new Intent(mContext, SetupSyncActivity.class);
        mContext.startActivity(intent);
    }
}
