/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.updater;

import android.content.Intent;
import android.support.annotation.NonNull;

public class UpdatesApplyService extends UpdaterService {
    @Override
    protected void onHandleWork(@NonNull Intent intent) {
        // The apply action from the notification while the download is in progress is
        // treated in the inner BroadcastReceiver of the service which posted the notification
        updater.applyUpdate(intent.getStringExtra(UpdateServiceHelper.EXTRA_PACKAGE_PATH_NAME));
    }
}
