/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.restrictions;

import org.mozilla.gecko.AppConstants;

import android.annotation.TargetApi;
import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.RestrictionEntry;
import android.os.Build;
import android.os.Bundle;

import java.util.ArrayList;

/**
 * Broadcast receiver providing supported restrictions to the system.
 */
@TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
public class RestrictionProvider extends BroadcastReceiver {
    @Override
    public void onReceive(final Context context, final Intent intent) {
        if (AppConstants.Versions.preJBMR2) {
            // This broadcast does not make any sense prior to Jelly Bean MR2.
            return;
        }

        final PendingResult result = goAsync();

        new Thread() {
            @Override
            public void run() {
                final Bundle oldRestrictions = intent.getBundleExtra(Intent.EXTRA_RESTRICTIONS_BUNDLE);
                RestrictedProfileConfiguration.migrateRestrictionsIfNeeded(oldRestrictions);

                final Bundle extras = new Bundle();

                ArrayList<RestrictionEntry> entries = initRestrictions(context, oldRestrictions);
                extras.putParcelableArrayList(Intent.EXTRA_RESTRICTIONS_LIST, entries);

                result.setResult(Activity.RESULT_OK, null, extras);
                result.finish();
            }
        }.start();
    }

    private ArrayList<RestrictionEntry> initRestrictions(Context context, Bundle oldRestrictions) {
        ArrayList<RestrictionEntry> entries = new ArrayList<RestrictionEntry>();

        for (Restrictable restrictable : RestrictedProfileConfiguration.DEFAULT_DISABLED_FEATURES) {
            if (restrictable == Restrictable.LOCATION_SERVICE && !AppConstants.MOZ_STUMBLER_BUILD_TIME_ENABLED) {
                continue;
            }

            RestrictionEntry entry = createRestrictionEntryWithDefaultValue(context, restrictable,
                    oldRestrictions.getBoolean(restrictable.name, false));
            entries.add(entry);
        }

        return entries;
    }

    private RestrictionEntry createRestrictionEntryWithDefaultValue(Context context, Restrictable restrictable, boolean defaultValue) {
        RestrictionEntry entry = new RestrictionEntry(restrictable.name, defaultValue);

        entry.setTitle(restrictable.getTitle(context));

        return entry;
    }
}
