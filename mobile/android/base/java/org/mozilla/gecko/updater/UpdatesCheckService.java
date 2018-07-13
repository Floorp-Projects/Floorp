/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.updater;

import android.content.Intent;
import android.support.annotation.NonNull;

public class UpdatesCheckService extends UpdaterService {
    @Override
    protected void onHandleWork(@NonNull Intent intent) {
        updater.startUpdate(intent.getIntExtra(UpdateServiceHelper.EXTRA_UPDATE_FLAGS_NAME, 0));

        // Use this instead for forcing a download from about:fennec
        // startUpdate(UpdateServiceHelper.FLAG_FORCE_DOWNLOAD | UpdateServiceHelper.FLAG_REINSTALL);
    }
}
