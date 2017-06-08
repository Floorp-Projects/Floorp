/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.telemetry;

import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.util.Log;

import org.mozilla.gecko.sync.CollectionConcurrentModificationException;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.HTTPFailureException;
import org.mozilla.gecko.sync.SyncDeadlineReachedException;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.net.SyncStorageResponse;
import org.mozilla.gecko.sync.repositories.FetchFailedException;
import org.mozilla.gecko.sync.repositories.StoreFailedException;
import org.mozilla.gecko.sync.repositories.domain.ClientRecord;

import java.io.UnsupportedEncodingException;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.HashMap;

/**
 * Gathers telemetry about a single run of sync.
 * In light of sync restarts, rarely a "single sync" will actually include more than one sync.
 * See {@link TelemetryStageCollector} for "stage telemetry".
 *
 * @author grisha
 */
public class TelemetryCollector {
    private static final String LOG_TAG = "TelemetryCollector";

    public static final String KEY_ERROR_INTERNAL = "internal";
    public static final String KEY_ERROR_TOKEN = "token";

    // Telemetry collected by individual stages is aggregated here. Stages run sequentially,
    // and only access their own collectors.
    private final HashMap<String, TelemetryStageCollector> stageCollectors = new HashMap<>();

    // Data which is not specific to a single stage is aggregated in this object.
    // It's possible that these fields are read/written from different threads.
    // Volatile is used to ensure memory visibility.
    private volatile ExtendedJSONObject error;
    private volatile String hashedUID;
    private volatile String hashedDeviceID;
    private final ArrayList<Bundle> devices = new ArrayList<>();

    @Nullable private volatile Long started;
    @Nullable private volatile Long finished;

    private volatile boolean didRestart = false;

    public TelemetryStageCollector collectorFor(@NonNull String stageName) {
        if (stageCollectors.containsKey(stageName)) {
            return stageCollectors.get(stageName);
        }

        final TelemetryStageCollector collector = new TelemetryStageCollector(this);
        stageCollectors.put(stageName, collector);
        return collector;
    }

    public void setRestarted() {
        this.didRestart = true;
    }

    public void setIDs(@NonNull String uid, @NonNull String deviceID) {
        // We use hashed_fxa_uid from the token server as our UID.
        this.hashedUID = uid;
        try {
            this.hashedDeviceID = Utils.byte2Hex(Utils.sha256(
                            deviceID.concat(uid).getBytes("UTF-8")
                    ));
        } catch (UnsupportedEncodingException | NoSuchAlgorithmException e) {
            // Should not happen.
            Log.e(LOG_TAG, "Either UTF-8 or SHA-256 are not supported", e);
        }
    }

    public void setError(@NonNull String name, @NonNull Exception e) {
        setError(name, e.getClass().getSimpleName());
    }

    public void setError(@NonNull String name, @NonNull String details) {
        setError(name, details, null);
    }

    public void setError(@NonNull String name, @NonNull String details, @Nullable Exception e) {
        final ExtendedJSONObject error = new ExtendedJSONObject();
        error.put("name", name);
        if (e != null) {
            error.put("error", e.getClass().getSimpleName() + ":" + details);
        } else {
            error.put("error", details);
        }
        this.error = error;
    }

    public boolean hasError() {
        return this.error != null;
    }

    public void setStarted(long time) {
        this.started = time;
    }

    public void setFinished(long time) {
        this.finished = time;
    }

    // In the current sync-ping parlance, "device" is really just a sync client.
    // At some point we will start recording actual FxA devices.
    public void addDevice(final ClientRecord client) {
        if (this.hashedUID == null) {
            throw new IllegalStateException("Must call setIDs before adding devices.");
        }

        final Bundle device = new Bundle();
        device.putString(TelemetryContract.KEY_DEVICE_OS, client.os);
        device.putString(TelemetryContract.KEY_DEVICE_VERSION, client.version);

        final String clientAndUid = client.guid.concat(this.hashedUID);
        try {
            device.putString(
                    TelemetryContract.KEY_DEVICE_ID,
                    Utils.byte2Hex(Utils.sha256(clientAndUid.getBytes("UTF-8")))
            );
        } catch (UnsupportedEncodingException | NoSuchAlgorithmException e) {
            // Should not happen.
            Log.e(LOG_TAG, "Either UTF-8 or SHA-256 are not supported", e);
        }
        devices.add(device);
    }

    public Bundle build() {
        if (this.started == null) {
            throw new IllegalStateException("Telemetry missing 'started' timestamp");
        }
        if (this.finished == null) {
            throw new IllegalStateException("Telemetry missing 'finished' timestamp");
        }

        final long took = this.finished - this.started;

        final Bundle telemetry = new Bundle();
        telemetry.putString(TelemetryContract.KEY_LOCAL_UID, this.hashedUID);
        telemetry.putString(TelemetryContract.KEY_LOCAL_DEVICE_ID, this.hashedDeviceID);
        telemetry.putParcelableArrayList(TelemetryContract.KEY_DEVICES, this.devices);
        telemetry.putLong(TelemetryContract.KEY_TOOK, took);
        if (this.error != null) {
            telemetry.putSerializable(TelemetryContract.KEY_ERROR, this.error.object);
        }
        telemetry.putSerializable(TelemetryContract.KEY_STAGES, this.stageCollectors);
        if (this.didRestart) {
            telemetry.putBoolean(TelemetryContract.KEY_RESTARTED, true);
        }
        return telemetry;
    }

    /**
     * Builder class which is responsible for mapping instances of exceptions thrown during sync
     * stages into a JSON structure that may be submitted as part of a sync ping.
     */
    public static class StageErrorBuilder {
        @Nullable private Exception lastException;
        @Nullable Exception storeException;
        @Nullable Exception fetchException;

        @Nullable private final String name;
        @Nullable private final String error;

        public StageErrorBuilder() {
            this(null, null);
        }

        public StageErrorBuilder(@Nullable String name, @Nullable String error) {
            this.name = name;
            this.error = error;
        }

        public StageErrorBuilder setLastException(Exception e) {
            lastException = e;
            return this;
        }

        public StageErrorBuilder setStoreException(Exception e) {
            storeException = e;
            return this;
        }

        public StageErrorBuilder setFetchException(Exception e) {
            fetchException = e;
            return this;
        }

        // Unlike the rest of TelemetryCollector, which only vaguely hints at the particulars of a
        // sync ping, this method contains specific details - naming of keys/values, etc.
        // This is done consciously and for simplicity's sake. The alternative is to either pack
        // these key/values behind an interface and unpack them on the receiver end, or let the receiver
        // figure out how to deal with exceptions directly. Either way, we'll have a strong coupling.
        public ExtendedJSONObject build() {
            final ExtendedJSONObject errorJSON = new ExtendedJSONObject();

            // Process manually set name, error and optional exception.
            if (name != null && error != null) {
                errorJSON.put("name", name);
                errorJSON.put("error", error);

                if (lastException != null && lastException instanceof HTTPFailureException) {
                    final SyncStorageResponse response = ((HTTPFailureException)lastException).response;
                    errorJSON.put("code", response.getStatusCode());
                }

                return errorJSON;
            }

            // Process set exceptions.
            if (lastException instanceof CollectionConcurrentModificationException) {
                errorJSON.put("name", "httperror");
                errorJSON.put("code", 412);

            } else if (lastException instanceof SyncDeadlineReachedException) {
                errorJSON.put("name", "unexpected");
                errorJSON.put("error", "syncdeadline");

            } else if (lastException instanceof FetchFailedException) {
                if (isNetworkError(fetchException)) {
                    errorJSON.put("name", "networkerror");
                    errorJSON.put("error", "fetch:" + fetchException.getClass().getSimpleName());
                } else {
                    errorJSON.put("name", "othererror");
                    if (fetchException != null) {
                        errorJSON.put("error", "fetch:" + fetchException.getClass().getSimpleName());
                    } else {
                        errorJSON.put("error", "fetch:unknown");
                    }
                }

            } else if (lastException instanceof StoreFailedException) {
                if (isNetworkError(storeException)) {
                    errorJSON.put("name", "networkerror");
                    errorJSON.put("error", "store:" + storeException.getClass().getSimpleName());

                // Currently we only have access to one exception, the last one that happened. However, there
                // could have been multiple errors that we're currently ignoring. See Bug 1362208.
                // Local store failures are ignored (but will get recorded in the incoming failure count).
                // Remote store failures generally do not abort the session, but will bubble up as an error.
                // See Bug 1362206.
                } else {
                    errorJSON.put("name", "othererror");
                    if (storeException != null) {
                        errorJSON.put("error", "store:" + storeException.getClass().getSimpleName());
                    } else {
                        errorJSON.put("error", "store:unknown");
                    }
                }

            } else if (lastException instanceof HTTPFailureException) {
                final SyncStorageResponse response = ((HTTPFailureException)lastException).response;

                // Is it an auth error? This could be a password change or a node re-assignment, and
                // we can't distinguish between the two until we fetch new cluster URL during the
                // next sync session.
                if (response.getStatusCode() == 401) {
                    errorJSON.put("name", "autherror");
                    // Desktop clients differentiate between "tokenserver", "hawkclient", and "fxaccounts".
                    // We will encounter this error during a sync stage run, and so it will come
                    // from a sync storage node.
                    errorJSON.put("from", "storage");
                } else {
                    errorJSON.put("name", "httperror");
                    errorJSON.put("code", response.getStatusCode());
                }

            } else if (lastException != null) {
                errorJSON.put("name", "unexpected");
                errorJSON.put("error", lastException.getClass().getSimpleName());
            }

            return errorJSON;
        }

        private static boolean isNetworkError(@Nullable Exception e) {
            return e instanceof java.net.SocketException;
        }
    }
}
