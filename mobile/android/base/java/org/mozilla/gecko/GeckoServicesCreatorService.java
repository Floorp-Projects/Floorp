/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;
import android.content.Intent;
import android.support.annotation.NonNull;

public class GeckoServicesCreatorService extends GeckoService {
    private static final String LOGTAG = "GeckoSrvCreatorService";

    @Override
    protected void onHandleWork(@NonNull Intent intent) {
        if (!isStartingIntentValid(intent, INTENT_ACTION_CREATE_SERVICES)) {
            return;
        }

        if (!initGecko(intent)) {
            return;
        }

        final String category = intent.getStringExtra(INTENT_SERVICE_CATEGORY);
        final String data = intent.getStringExtra(INTENT_SERVICE_DATA);

        if (category == null) {
            return;
        }

        GeckoThread.createServices(category, data);
    }

    public static void enqueueWork(@NonNull final Context context, @NonNull final Intent workIntent) {
        enqueueWork(context, GeckoServicesCreatorService.class, JobIdsConstants.getIdForGeckoServicesCreator(), workIntent);
    }
}
