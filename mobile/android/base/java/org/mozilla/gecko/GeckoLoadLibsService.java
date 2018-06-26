/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;
import android.content.Intent;
import android.support.annotation.NonNull;

public class GeckoLoadLibsService extends GeckoService {
    @Override
    protected void onHandleWork(@NonNull Intent intent) {
        // Intentionally not initialize Gecko when only loading libs.
        // All work this service does is in onCreate()
    }

    public static void enqueueWork(@NonNull final Context context, @NonNull final Intent workIntent) {
        enqueueWork(context, GeckoLoadLibsService.class, JobIdsConstants.getIdForGeckoLibsLoader(), workIntent);
    }
}
