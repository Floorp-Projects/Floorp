/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.preferences;

import org.mozilla.gecko.sync.setup.SyncAccounts;
import org.mozilla.gecko.sync.setup.activities.SetupSyncActivity;

import android.content.Context;
import android.content.Intent;
import android.preference.Preference;
import android.util.AttributeSet;

class SyncPreference extends Preference {
    private Context mContext;

    public SyncPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
    }

    @Override
    protected void onClick() {
        // Show Sync setup if no accounts exist; otherwise, show account settings.
        if (SyncAccounts.syncAccountsExist(mContext)) {
            SyncAccounts.openSyncSettings(mContext);
        } else {
            Intent intent = new Intent(mContext, SetupSyncActivity.class);
            mContext.startActivity(intent);
        }
    }
}
