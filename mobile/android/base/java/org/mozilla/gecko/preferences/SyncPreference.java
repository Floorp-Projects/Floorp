/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.preferences;

import android.content.Context;
import android.content.Intent;
import android.preference.Preference;
import android.text.TextUtils;
import android.util.AttributeSet;

import com.squareup.picasso.Picasso;
import com.squareup.picasso.Target;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.TelemetryContract.Method;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.activities.FxAccountWebFlowActivity;
import org.mozilla.gecko.fxa.activities.PicassoPreferenceIconTarget;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.util.ThreadUtils;

class SyncPreference extends Preference {
    private final Context mContext;
    private final Target profileAvatarTarget;

    public SyncPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
        final float cornerRadius = mContext.getResources().getDimension(R.dimen.fxaccount_profile_image_width) / 2;
        profileAvatarTarget = new PicassoPreferenceIconTarget(mContext.getResources(), this, cornerRadius);
    }

    private void launchFxASetup() {
        final Intent intent = new Intent(FxAccountConstants.ACTION_FXA_GET_STARTED);
        intent.putExtra(FxAccountWebFlowActivity.EXTRA_ENDPOINT, FxAccountConstants.ENDPOINT_PREFERENCES);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
        mContext.startActivity(intent);
    }

    public void update(final AndroidFxAccount fxAccount) {
        if (fxAccount == null) {
            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    setTitle(R.string.pref_sync);
                    setSummary(R.string.pref_sync_summary);
                        // Cancel any pending task.
                        Picasso.with(mContext).cancelRequest(profileAvatarTarget);
                        // Clear previously set icon.
                        // Bug 1312719 - IconDrawable is prior to IconResId, drawable must be set null before setIcon(resId)
                        // http://androidxref.com/5.1.1_r6/xref/frameworks/base/core/java/android/preference/Preference.java#562
                        setIcon(null);
                        setIcon(R.drawable.sync_avatar_default);
                }
            });
            return;
        }

        // Update title from account email.
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                setTitle(fxAccount.getEmail());
                setSummary("");
            }
        });

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
        // Launch the FxA "Get started" activity, which will dispatch to the
        // right location.
        launchFxASetup();
        Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, Method.SETTINGS, "sync_setup");
    }
}
