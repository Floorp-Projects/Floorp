/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.updater;

import android.content.Intent;
import android.support.annotation.NonNull;

import org.mozilla.gecko.updater.UpdateServiceHelper.AutoDownloadPolicy;

public class UpdatesRegisterService extends UpdaterService {
    @Override
    protected void onHandleWork(@NonNull Intent intent) {
        AutoDownloadPolicy policy = AutoDownloadPolicy.get(
                intent.getIntExtra(UpdateServiceHelper.EXTRA_AUTODOWNLOAD_NAME,
                        AutoDownloadPolicy.NONE.value));

        if (policy != AutoDownloadPolicy.NONE) {
            prefs.setAutoDownloadPolicy(policy);
        }

        String url = intent.getStringExtra(UpdateServiceHelper.EXTRA_UPDATE_URL_NAME);
        if (url != null) {
            prefs.setUpdateUrl(url);
        }

        updater.registerForUpdates(false);
    }
}
