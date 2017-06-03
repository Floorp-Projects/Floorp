/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.telemetry.schedulers;

import android.content.Context;
import org.mozilla.gecko.telemetry.stores.TelemetryPingStore;

/**
 * An implementation of this class can investigate the given {@link TelemetryPingStore} to
 * decide if it's ready to upload the pings inside that Store (e.g. on wifi? have we
 * accumulated X pings?) and can schedule that upload. Typically, the upload will be
 * scheduled by sending an {@link android.content.Intent} to the
 * {@link org.mozilla.gecko.telemetry.TelemetryUploadService}, either immediately or
 * via an external scheduler (e.g. {@link android.app.job.JobScheduler}).
 *
 * N.B.: If the Store is not ready to upload, an implementation *should not* try to reschedule
 * the check to see if it's time to upload - this is expected to be handled by the caller.
 */
public interface TelemetryUploadScheduler {
    boolean isReadyToUpload(Context applicationContext, TelemetryPingStore store);
    void scheduleUpload(Context applicationContext, TelemetryPingStore store);
}
