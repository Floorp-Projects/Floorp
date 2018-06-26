/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;
import android.content.Intent;
import android.support.annotation.NonNull;

/**
 * Service used to try and initialize Gecko
 */
public class GeckoStarterService extends GeckoService {
    private static final String LOGTAG = "GeckoStarterService";

    @Override
    protected void onHandleWork(@NonNull Intent intent) {
        if (!isStartingIntentValid(intent, INTENT_ACTION_START_GECKO)) {
            return;
        }

        initGecko(intent);
    }

    public static void enqueueWork(@NonNull final Context context, @NonNull final Intent workIntent) {
        enqueueWork(context, GeckoStarterService.class, JobIdsConstants.getIdForGeckoStarter(), workIntent);
    }
}
