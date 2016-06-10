/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.telemetry.stores;

import android.os.Parcelable;
import org.mozilla.gecko.telemetry.TelemetryPing;

import java.io.IOException;
import java.util.List;
import java.util.Set;

/**
 * Persistent storage for TelemetryPings that are queued for upload.
 *
 * An implementation of this class is expected to be thread-safe. Additionally,
 * multiple instances can be created and run simultaneously so they must be able
 * to synchronize state (or be stateless!).
 *
 * The pings in {@link #getAllPings()} and {@link #maybePrunePings()} are returned in the
 * same order in order to guarantee consistent results.
 */
public abstract class TelemetryPingStore implements Parcelable {
    private final String profileName;

    public TelemetryPingStore(final String profileName) {
        this.profileName = profileName;
    }

    /**
     * @return the profile name associated with this store.
     */
    public String getProfileName() {
        return profileName;
    }

    /**
     * @return a list of all the telemetry pings in the store that are ready for upload, ascending oldest to newest.
     */
    public abstract List<TelemetryPing> getAllPings();

    /**
     * Save a ping to the store.
     *
     * @param ping the ping to store
     * @throws IOException for underlying store access errors
     */
    public abstract void storePing(TelemetryPing ping) throws IOException;

    /**
     * Removes telemetry pings from the store if there are too many pings or they take up too much space.
     * Pings should be removed from oldest to newest.
     */
    public abstract void maybePrunePings();

    /**
     * Removes the successfully uploaded pings from the database and performs another other actions necessary
     * for when upload is completed.
     *
     * @param successfulRemoveIDs doc ids of pings that were successfully uploaded
     */
    public abstract void onUploadAttemptComplete(Set<String> successfulRemoveIDs);
}
