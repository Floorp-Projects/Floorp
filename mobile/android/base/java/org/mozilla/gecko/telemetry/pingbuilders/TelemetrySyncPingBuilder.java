/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.telemetry.pingbuilders;

import android.os.Bundle;
import android.os.Parcelable;
import android.support.annotation.NonNull;

import org.json.simple.JSONArray;
import org.json.simple.JSONObject;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.telemetry.TelemetryContract;
import org.mozilla.gecko.sync.telemetry.TelemetryStageCollector;
import org.mozilla.gecko.telemetry.TelemetryLocalPing;

import java.io.Serializable;
import java.util.ArrayList;
import java.util.HashMap;

/**
 * Local ping builder which understands how to process sync data.
 * Whenever hashing of data is involved, we expect it to be performed at the time of collection,
 * somewhere in {@link org.mozilla.gecko.sync.telemetry.TelemetryCollector} and friends.
 */
public class TelemetrySyncPingBuilder extends TelemetryLocalPingBuilder {
    public TelemetrySyncPingBuilder setStages(@NonNull final Serializable data) {
        HashMap<String, TelemetryStageCollector> stages = castSyncData(data);

        final JSONArray engines = new JSONArray();
        for (String stageName : stages.keySet()) {
            final TelemetryStageCollector stage = stages.get(stageName);

            // Skip stages that did nothing.
            if (stage.inbound == 0 && stage.outbound == 0) {
                continue;
            }

            final ExtendedJSONObject stageJSON = new ExtendedJSONObject();

            stageJSON.put("name", stageName);
            stageJSON.put("took", stage.finished - stage.started);

            // Desktop also includes a "status" field with internal constants as possible values.
            // Status may be deducted by inspecting 'failureReason', and as such it is omitted here.
            // Absence of 'failureReason' means that stage succeeded.

            if (stage.inbound > 0) {
                final ExtendedJSONObject incomingJSON = new ExtendedJSONObject();
                incomingJSON.put("applied", stage.inbound);
                if (stage.inboundStored > 0) {
                    incomingJSON.put("succeeded", stage.inboundStored);
                }
                if (stage.inboundFailed > 0) {
                    incomingJSON.put("failed", stage.inboundFailed);
                }
                if (stage.reconciled > 0) {
                    incomingJSON.put("reconciled", stage.reconciled);
                }
                stageJSON.put("incoming", incomingJSON);
            }

            if (stage.outbound > 0) {
                final ExtendedJSONObject outgoingJSON = new ExtendedJSONObject();
                // We specifically do not check if `outboundStored` is greater than zero.
                // `outbound` schema is simpler than `inbound`, namely there isn't an "attempted
                // to send" count.
                // Stage telemetry itself has that data (outbound = outboundStored + outboundFailed),
                // and so this is our way to relay slightly more information.
                // e.g. we'll know there's something wrong if `sent = 0` and `failed` is missing.
                outgoingJSON.put("sent", stage.outboundStored);
                if (stage.outboundFailed > 0) {
                    outgoingJSON.put("failed", stage.outboundFailed);
                }
                stageJSON.put("outgoing", outgoingJSON);
            }

            // We depend on the error builder from TelemetryCollector to produce the right schema.
            // Spreading around our schema definition like that is awkward, but, alas, here we are.
            if (stage.error != null) {
                stageJSON.put("failureReason", stage.error);
            }

            addUnchecked(engines, stageJSON);
        }
        payload.put("engines", engines);
        return this;
    }

    public TelemetrySyncPingBuilder setUID(@NonNull String uid) {
        payload.put("uid", uid);
        return this;
    }

    public TelemetrySyncPingBuilder setDeviceID(@NonNull String deviceID) {
        payload.put("deviceID", deviceID);
        return this;
    }

    public TelemetrySyncPingBuilder setRestarted(boolean didRestart) {
        if (!didRestart) {
            return this;
        }

        payload.put("restarted", true);
        return this;
    }

    public TelemetrySyncPingBuilder setDevices(@NonNull ArrayList<Parcelable> devices) {
        final JSONArray devicesJSON = new JSONArray();

        for (Parcelable device : devices) {
            final Bundle deviceBundle = (Bundle) device;
            final ExtendedJSONObject deviceJSON = new ExtendedJSONObject();

            deviceJSON.put("os", deviceBundle.getString(TelemetryContract.KEY_DEVICE_OS));
            deviceJSON.put("version", deviceBundle.getString(TelemetryContract.KEY_DEVICE_VERSION));
            deviceJSON.put("id", deviceBundle.getString(TelemetryContract.KEY_DEVICE_ID));

            addUnchecked(devicesJSON, deviceJSON);
        }

        if (devicesJSON.size() > 0) {
            payload.put("devices", devicesJSON);
        }
        return this;
    }

    public TelemetrySyncPingBuilder setError(@NonNull Serializable error) {
        payload.put("failureReason", new ExtendedJSONObject((JSONObject) error));
        return this;
    }

    public TelemetrySyncPingBuilder setTook(long took) {
        payload.put("took", took);
        return this;
    }

    @Override
    public TelemetryLocalPing build() {
        payload.put("when", System.currentTimeMillis());
        return new TelemetryLocalPing(payload, docID);
    }

    @SuppressWarnings("unchecked")
    private static void addUnchecked(final JSONArray list, final ExtendedJSONObject obj) {
        list.add(obj);
    }

    /**
     * We broadcast this data via LocalBroadcastManager and control both sides of this code, so it
     * is acceptable to do an unchecked cast.
     */
    @SuppressWarnings("unchecked")
    private static HashMap<String, TelemetryStageCollector> castSyncData(final Serializable data) {
        return (HashMap<String, TelemetryStageCollector>) data;
    }
}
