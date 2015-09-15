/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.preferences;

import android.content.Context;
import android.content.Intent;
import android.preference.Preference;
import android.util.AttributeSet;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.TelemetryContract.Method;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.sync.setup.SyncAccounts;
import org.mozilla.gecko.sync.setup.activities.SetupSyncActivity;

class SyncPreference extends Preference {
    private static final boolean DEFAULT_TO_FXA = true;

    private final Context mContext;

    public SyncPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
    }

    private void openSync11Settings() {
        // Show Sync setup if no accounts exist; otherwise, show account settings.
        if (SyncAccounts.syncAccountsExist(mContext)) {
            // We don't check for failure here. If you already have Sync set up,
            // then there's nothing we can do.
            SyncAccounts.openSyncSettings(mContext);
            return;
        }
        Intent intent = new Intent(mContext, SetupSyncActivity.class);
        mContext.startActivity(intent);
    }

    private void launchFxASetup() {
        final Intent intent = new Intent(FxAccountConstants.ACTION_FXA_GET_STARTED);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
        mContext.startActivity(intent);
    }

    @Override
    protected void onClick() {
        // If we're not defaulting to FxA, just do what we've always done.
        if (!DEFAULT_TO_FXA) {
            openSync11Settings();
            return;
        }

        // If there's a legacy Sync account (or a pickled one on disk),
        // open the settings page.
        if (SyncAccounts.syncAccountsExist(mContext)) {
            if (SyncAccounts.openSyncSettings(mContext) != null) {
                Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, Method.SETTINGS, "sync_settings");
                return;
            }
        }

        // Otherwise, launch the FxA "Get started" activity, which will
        // dispatch to the right location.
        launchFxASetup();
        Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, Method.SETTINGS, "sync_setup");
    }
}
