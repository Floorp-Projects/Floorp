/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.preferences;

import android.accounts.Account;
import android.content.Context;
import android.content.Intent;
import android.preference.Preference;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.View;
import android.widget.TextView;

import com.squareup.picasso.Picasso;
import com.squareup.picasso.Target;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.TelemetryContract.Method;
import org.mozilla.gecko.fxa.FirefoxAccounts;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.activities.FxAccountWebFlowActivity;
import org.mozilla.gecko.fxa.activities.PicassoPreferenceIconTarget;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.setup.SyncAccounts;
import org.mozilla.gecko.sync.setup.activities.SetupSyncActivity;

class SyncPreference extends Preference {
    private static final boolean DEFAULT_TO_FXA = true;

    private final Context mContext;
    private final Target profileAvatarTarget;

    public SyncPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
        final float cornerRadius = mContext.getResources().getDimension(R.dimen.fxaccount_profile_image_width) / 2;
        profileAvatarTarget = new PicassoPreferenceIconTarget(mContext.getResources(), this, cornerRadius);
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
        intent.putExtra(FxAccountWebFlowActivity.EXTRA_ENDPOINT, FxAccountConstants.ENDPOINT_PREFERENCES);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
        mContext.startActivity(intent);
    }

    @Override
    protected void onBindView(View view) {
        super.onBindView(view);

        Account account = FirefoxAccounts.getFirefoxAccount(mContext);
        if (account == null) {
            return;
        }

        final AndroidFxAccount fxAccount = new AndroidFxAccount(mContext, account);
        final TextView title = (TextView) view.findViewById(android.R.id.title);
        final TextView summary = (TextView) view.findViewById(android.R.id.summary);
        title.setText(fxAccount.getEmail());
        summary.setVisibility(View.GONE);

        // Updating icons from Java is not supported prior to API 11.
        if (!AppConstants.Versions.feature11Plus) {
            return;
        }

        final ExtendedJSONObject profileJSON = fxAccount.getProfileJSON();
        if (profileJSON == null) {
            return;
        }

        // Avatar URI empty, return early.
        final String avatarURI = profileJSON.getString(FxAccountConstants.KEY_PROFILE_JSON_AVATAR);
        if (TextUtils.isEmpty(avatarURI)) {
            return;
        }

        Picasso.with(mContext)
                .load(avatarURI)
                .centerInside()
                .resizeDimen(R.dimen.fxaccount_profile_image_width, R.dimen.fxaccount_profile_image_height)
                .placeholder(R.drawable.sync_avatar_default)
                .error(R.drawable.sync_avatar_default)
                .into(profileAvatarTarget);
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
