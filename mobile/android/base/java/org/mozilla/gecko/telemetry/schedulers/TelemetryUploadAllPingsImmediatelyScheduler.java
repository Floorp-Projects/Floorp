/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.telemetry.schedulers;

import android.content.Context;
import android.content.Intent;
import org.mozilla.gecko.telemetry.stores.TelemetryPingStore;
import org.mozilla.gecko.telemetry.TelemetryUploadService;

/**
 * Schedules an upload with all pings to be sent immediately.
 */
public class TelemetryUploadAllPingsImmediatelyScheduler implements TelemetryUploadScheduler {

    @Override
    public boolean isReadyToUpload(final TelemetryPingStore store) {
        // We're ready since we don't have any conditions to wait on (e.g. on wifi, accumulated X pings).
        return true;
    }

    @Override
    public void scheduleUpload(final Context context, final TelemetryPingStore store) {
        final Intent i = new Intent(TelemetryUploadService.ACTION_UPLOAD);
        i.setClass(context, TelemetryUploadService.class);
        i.putExtra(TelemetryUploadService.EXTRA_STORE, store);
        context.startService(i);
    }
}
