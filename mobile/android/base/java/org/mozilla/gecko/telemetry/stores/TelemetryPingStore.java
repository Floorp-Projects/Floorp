/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.telemetry.stores;

import android.os.Parcelable;
import org.mozilla.gecko.telemetry.TelemetryPing;
import org.mozilla.gecko.telemetry.TelemetryPingFromStore;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Set;

/**
 * Persistent storage for TelemetryPings that are queued for upload.
 *
 * An implementation of this class is expected to be thread-safe.
 */
public interface TelemetryPingStore extends Parcelable {

    /**
     * @return a list of all the telemetry pings in the store that are ready for upload.
     */
    ArrayList<TelemetryPingFromStore> getAllPings();

    /**
     * Save a ping to the store.
     *
     * @param uniqueID a unique identifier for the ping - will be returned in {@link #onUploadAttemptComplete(Set)}
     * @param ping the ping to store
     * @throws IOException for underlying store access errors
     */
    void storePing(long uniqueID, TelemetryPing ping) throws IOException;

    /**
     * Removes telemetry pings from the store if there are too many pings or they take up too much space.
     */
    void maybePrunePings();

    /**
     * Removes the successfully uploaded pings from the database and performs another other actions necessary
     * for when upload is completed.
     *
     * @param successfulRemoveIDs unique ids of pings passed to {@link #storePing(long, TelemetryPing)} that were
     *                            successfully uploaded
     */
    void onUploadAttemptComplete(Set<Integer> successfulRemoveIDs);
}
