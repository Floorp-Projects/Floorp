/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.telemetry;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.os.Parcelable;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.content.LocalBroadcastManager;
import android.util.Log;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.Experiments;
import org.mozilla.gecko.switchboard.SwitchBoard;
import org.mozilla.gecko.sync.telemetry.TelemetryContract;
import org.mozilla.gecko.telemetry.pingbuilders.TelemetrySyncEventPingBuilder;
import org.mozilla.gecko.telemetry.pingbuilders.TelemetrySyncPingBuilder;
import org.mozilla.gecko.telemetry.pingbuilders.TelemetrySyncPingBundleBuilder;
import org.mozilla.gecko.telemetry.schedulers.TelemetryUploadAllPingsImmediatelyScheduler;
import org.mozilla.gecko.telemetry.schedulers.TelemetryUploadScheduler;
import org.mozilla.gecko.telemetry.stores.TelemetryJSONFilePingStore;
import org.mozilla.gecko.telemetry.stores.TelemetryPingStore;
import org.mozilla.gecko.util.ThreadUtils;

import java.io.IOException;
import java.io.Serializable;
import java.util.ArrayList;
import java.util.Set;

/**
 * Receives and processes telemetry broadcasts from background services, namely Sync.
 * Nomenclature:
 * - Bundled Sync Ping: a Sync Ping as documented at http://gecko.readthedocs.io/en/latest/toolkit/components/telemetry/telemetry/data/sync-ping.html
 *   as of commit https://github.com/mozilla-services/docs/commit/7eb4b412d3ab5ec46b280eff312ace32e7cf27e6
 * - Telemetry data: incoming background telemetry, of two types: "sync" and "sync event"
 * - Local Sync Ping: a persistable representation of incoming telemetry data. Not intended for upload.
 *   See {@link TelemetryLocalPing}
 *
 * General flow:
 * - background telemetry bundles come in, describing syncs or events that happened
 * - telemetry bundles are transformed into a local pings and persisted
 * - once there are enough local pings, or another upload condition kicks in, all of the persisted
 *   local pings are bundled into a single outgoing Sync Ping, removed from the local store,
 *   and Sync Ping is persisted and uploaded.
 *
 *   @author grisha
 */
public class TelemetryBackgroundReceiver extends BroadcastReceiver {
    // NB: spelling is to appease logger's limitation on sizes of tags.
    private final static String LOG_TAG = "TelemetryBgReceiver";

    private static final String ACTION_BACKGROUND_TELEMETRY = "org.mozilla.gecko.telemetry.BACKGROUND";
    private static final String SYNC_BUNDLE_STORE_DIR = "sync-ping-data";
    private static final String SYNC_STORE_DIR = "sync-data";
    private static final String SYNC_EVENT_STORE_DIR = "sync-event-data";
    private static final int LOCAL_SYNC_EVENT_PING_THRESHOLD = 100;
    private static final int LOCAL_SYNC_PING_THRESHOLD = 100;
    private static final long MAX_TIME_BETWEEN_UPLOADS = 12 * 60 * 60 * 1000; // 12 hours

    private static final String PREF_FILE_BACKGROUND_TELEMETRY = AppConstants.ANDROID_PACKAGE_NAME + ".telemetry.background";
    private static final String PREF_IDS = "ids";
    private static final String PREF_LAST_ATTEMPTED_UPLOADED = "last_attempted_upload";

    // We don't currently support passing profile along with background telemetry. Profile is used to
    // identify where pings are persisted locally.
    private static final String DEFAULT_PROFILE = "default";

    private static final TelemetryBackgroundReceiver instance = new TelemetryBackgroundReceiver();

    public static TelemetryBackgroundReceiver getInstance() {
        return instance;
    }

    public void init(Context context) {
        LocalBroadcastManager.getInstance(context).registerReceiver(
                this, new IntentFilter(ACTION_BACKGROUND_TELEMETRY));
    }

    @Override
    public void onReceive(final Context context, final Intent intent) {
        Log.i(LOG_TAG, "Handling background telemetry broadcast");

        // This is our kill-switch for background telemetry (or a functionality throttle).
        if (!SwitchBoard.isInExperiment(context, Experiments.ENABLE_PROCESSING_BACKGROUND_TELEMETRY)) {
            Log.i(LOG_TAG, "Background telemetry processing disabled via switchboard.");
            return;
        }

        if (!intent.hasExtra(TelemetryContract.KEY_TELEMETRY)) {
            throw new IllegalStateException("Received a background telemetry broadcast without data.");
        }

        if (!intent.hasExtra(TelemetryContract.KEY_TYPE)) {
            throw new IllegalStateException("Received a background telemetry broadcast without type.");
        }

        // We want to know if any of the below code is faulty in non-obvious ways, and as such there
        // isn't an overarching try/catch to silence the errors.
        // That is, let's crash here if something goes really wrong, and hope that we'll spot the
        // error in the crash stats.
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                final String type = intent.getStringExtra(TelemetryContract.KEY_TYPE);

                // Setup local telemetry stores.
                final TelemetryJSONFilePingStore syncTelemetryStore = new TelemetryJSONFilePingStore(
                        context.getFileStreamPath(SYNC_STORE_DIR), DEFAULT_PROFILE);
                final TelemetryJSONFilePingStore syncEventTelemetryStore = new TelemetryJSONFilePingStore(
                        context.getFileStreamPath(SYNC_EVENT_STORE_DIR), DEFAULT_PROFILE);

                // Process incoming telemetry.
                final Bundle telemetryBundle = intent.getParcelableExtra(TelemetryContract.KEY_TELEMETRY);
                final String uid = telemetryBundle.getString(TelemetryContract.KEY_LOCAL_UID);
                final String deviceID = telemetryBundle.getString(TelemetryContract.KEY_LOCAL_DEVICE_ID);

                // Transform incoming telemetry into a local ping of correct type (sync vs event).
                final TelemetryLocalPing localPing;
                final TelemetryPingStore telemetryStore;
                switch (type) {
                    case TelemetryContract.KEY_TYPE_SYNC:
                        final ArrayList<Parcelable> devices = telemetryBundle.getParcelableArrayList(TelemetryContract.KEY_DEVICES);
                        final Serializable error = telemetryBundle.getSerializable(TelemetryContract.KEY_ERROR);
                        final Serializable stages = telemetryBundle.getSerializable(TelemetryContract.KEY_STAGES);
                        final long took = telemetryBundle.getLong(TelemetryContract.KEY_TOOK);
                        final boolean didRestart = telemetryBundle.getBoolean(TelemetryContract.KEY_RESTARTED);

                        telemetryStore = syncTelemetryStore;
                        TelemetrySyncPingBuilder localPingBuilder = new TelemetrySyncPingBuilder();

                        if (uid != null) {
                            localPingBuilder.setUID(uid);
                        }

                        if (deviceID != null) {
                            localPingBuilder.setDeviceID(deviceID);
                        }

                        if (devices != null) {
                            localPingBuilder.setDevices(devices);
                        }

                        if (stages != null) {
                            localPingBuilder.setStages(stages);
                        }

                        if (error != null) {
                            localPingBuilder.setError(error);
                        }

                        localPing = localPingBuilder
                                .setRestarted(didRestart)
                                .setTook(took)
                                .build();
                        break;
                    case TelemetryContract.KEY_TYPE_EVENT:
                        telemetryStore = syncEventTelemetryStore;
                        localPing = new TelemetrySyncEventPingBuilder()
                                .fromEventTelemetry(
                                        (Bundle) intent.getParcelableExtra(
                                                TelemetryContract.KEY_TELEMETRY))
                                .build();
                        break;
                    default:
                        throw new IllegalArgumentException("Unknown background telemetry type.");
                }

                // Persist the incoming telemetry data.
                try {
                    telemetryStore.storePing(localPing);
                } catch (IOException e) {
                    Log.e(LOG_TAG, "Could not store incoming telemetry. Attempting to upload already stored telemetry.", e);
                }

                // Determine if we should try uploading at this point, and attempt to do so.
                final SharedPreferences sharedPreferences = context.getSharedPreferences(
                        PREF_FILE_BACKGROUND_TELEMETRY, Context.MODE_PRIVATE);
                final TelemetryPingStore syncPingStore = new TelemetryJSONFilePingStore(
                        context.getFileStreamPath(SYNC_BUNDLE_STORE_DIR), DEFAULT_PROFILE);

                long lastAttemptedSyncPingUpload = sharedPreferences.getLong(PREF_LAST_ATTEMPTED_UPLOADED, 0L);

                // Changed IDs (uid or deviceID) is normally a reason to upload. However, if we
                // didn't receive uid or deviceID, we skip this check. This might happen if Sync
                // fails very early on, for example while talking to the token server.
                boolean idsChanged = false;
                if (uid != null && deviceID != null) {
                    idsChanged = setOrUpdateIDsIfChanged(sharedPreferences, uid, deviceID);
                }

                // Is there a good reason to upload at this time?
                final String reasonToUpload = reasonToUpload(
                        idsChanged,
                        syncTelemetryStore.getCount(),
                        syncEventTelemetryStore.getCount(),
                        lastAttemptedSyncPingUpload
                );

                // If we have a reason to upload at this point, bundle up sync and event telemetry
                // into a Sync Ping.
                if (reasonToUpload != null) {
                    // Get the IDs of telemetry objects we're about to bundle up.
                    // Note that this may race with other incoming background telemetry.
                    // We may accidentally drop non-bundled local telemetry if it was emitted while
                    // we're processing the current telemetry.
                    // Chances of that happening are very small due to how often background telemetry
                    // is actually emitted: very infrequently on a timescale of disk access.
                    final Set<String> localSyncTelemetryToRemove = syncTelemetryStore.getStoredIDs();
                    final Set<String> localSyncEventTelemetryToRemove = syncEventTelemetryStore.getStoredIDs();

                    // Bundle up all that we have in our telemetry stores.
                    final TelemetryOutgoingPing syncPing = new TelemetrySyncPingBundleBuilder()
                            .setSyncStore(syncTelemetryStore)
                            .setSyncEventStore(syncEventTelemetryStore)
                            .setReason(reasonToUpload)
                            .build();

                    // Persist a Sync Ping Bundle.
                    boolean bundledSyncPingPersisted = true;
                    try {
                        syncPingStore.storePing(syncPing);
                    } catch (IOException e) {
                        // If we fail to persist a bundled sync ping, we can either attempt to upload it,
                        // or skip the upload. Each choice has its own set of trade-offs.
                        // In short, current approach is to skip an upload. See Bug 1369186.
                        //
                        // If we choose to upload a Sync Ping that failed to persist locally, it becomes
                        // possible to upload the same telemetry multiple times. Since currently we do
                        // not have IDs as part of our sync and event telemetry objects, it is impossible
                        // to guarantee idempotence on the receiver's end. As such, we care to not
                        // upload the same thing multiple times. One way to achieve this involves
                        // creating an additional mapping of Bundled Sync Ping ID to two sets,
                        // {sync telemetry ids} and {event telemetry ids}, and taking care to only include
                        // telemetry in the subsequent pings which has not yet been associated with a
                        // Sync Ping Bundle. Given the fact that this will likely use a SharedPreference
                        // and that we inherently have a concurrent telemetry pipeline, it's quite possible
                        // that we'll get this wrong in some subtle way after considerable effort.
                        //
                        // An alternative is to simply not upload if we failed to persist a Sync Ping.
                        // This side-steps issues of idempotancy, but comes at a risk of taking longer
                        // to upload telemetry, or sometimes not uploading it at all - see Bug 1366045.
                        // However, those issues become likely only if our local JSON storage is failing
                        // frequently. This should not happen under most circumstances, and if it does
                        // happen frequently, we're likely to have a host of other problems.
                        //
                        // Yet another solution is to alter server-side processing of this data such that
                        // a unique ID may be included alongside each sync/event telemetry object. This
                        // will result in a very straightforward client implementation with much better
                        // consistency guarantees.
                        //
                        // See Bug 1368579 for exploring possible "runaway storage" implications of
                        // the current approach.
                        //
                        // Also note that a core assumption here is that storePing never successfully writes
                        // to disk if it throws.
                        Log.e(LOG_TAG, "Unable to write bundled sync ping to disk. Skipping upload.", e);
                        bundledSyncPingPersisted = false;
                    }

                    if (bundledSyncPingPersisted) {
                        // It is now safe to delete persisted telemetry which we just bundled up.
                        syncTelemetryStore.onUploadAttemptComplete(localSyncTelemetryToRemove);
                        syncEventTelemetryStore.onUploadAttemptComplete(localSyncEventTelemetryToRemove);
                    }
                }

                // Kick-off ping upload. If this succeeds, the uploader service will remove persisted
                // Sync Ping Bundles. Otherwise, we'll attempt another upload next time telemetry is
                // processed.
                // If we already have some persisted Sync Pings, that means a previous upload
                // failed - or, less likely, is in progress and did not yet succeed. It should be safe to
                // upload. Even if we raced with ourselves and uploaded some of the bundled sync pings more
                // than once, it's possible to guarantee idempotence on the receiver's end since we
                // include a unique ID with each ping. However, this depends on the telemetry pipeline
                // successfully de-duplicating submitted pings. As of Q2 2017, success rate of de-duping
                // sits around 45%, and is anticipated to be improved to 80-90% sometime by the end of 2017.
                // Relevant bugs are 1369512, 1357275.
                // Not uploading here means possibly delaying an already once-failed upload for a long
                // time. See Bug 1366045 for exploring scheduling options.
                if (reasonToUpload != null || syncPingStore.getCount() > 0) {
                    // Bump the "last-attempted-uploaded" timestamp, even though we might still fail
                    // to upload. Since we check for presence of pending pings above, if this upload
                    // fails we'll try again whenever next telemetry event happens.
                    sharedPreferences
                            .edit()
                            .putLong(PREF_LAST_ATTEMPTED_UPLOADED, System.currentTimeMillis())
                            .apply();

                    final TelemetryUploadScheduler scheduler = new TelemetryUploadAllPingsImmediatelyScheduler();
                    if (scheduler.isReadyToUpload(context, syncPingStore)) {
                        scheduler.scheduleUpload(context, syncPingStore);
                    }
                }
            }
        });
    }

    // There's no "scheduler" in a classic sense, and so we might end up not uploading pings at all
    // if there has been no new incoming telemetry data. See Bug 1366045.
    @Nullable
    protected static String reasonToUpload(boolean idsChanged, int syncCount, int eventCount, long lastUploadAttempt) {
        // Whenever any IDs change, upload.
        if (idsChanged) {
            return TelemetrySyncPingBundleBuilder.UPLOAD_REASON_IDCHANGE;
        }

        // Whenever we hit a certain threshold of local persisted telemetry, upload.
        if (syncCount > LOCAL_SYNC_PING_THRESHOLD || eventCount > LOCAL_SYNC_EVENT_PING_THRESHOLD) {
            return TelemetrySyncPingBundleBuilder.UPLOAD_REASON_COUNT;
        }

        final long now = System.currentTimeMillis();

        // If it's the first time we're processing telemetry data, upload ahead of schedule as a way
        // of saying "we're alive". This might often correspond to sending data about the first sync.
        if (lastUploadAttempt == 0L) {
            return TelemetrySyncPingBundleBuilder.UPLOAD_REASON_FIRST;
        }

        // Wall clock changed significantly; upload because we can't be sure of our timing anymore.
        // Allow for some wiggle room to account for clocks jumping around insignificantly.
        final long DRIFT_BUFFER_IN_MS = 60 * 1000L;
        if ((lastUploadAttempt - now) > DRIFT_BUFFER_IN_MS) {
            return TelemetrySyncPingBundleBuilder.UPLOAD_REASON_CLOCK_DRIFT;
        }

        // Upload if we haven't uploaded for some time.
        if ((now - lastUploadAttempt) >= MAX_TIME_BETWEEN_UPLOADS) {
            return TelemetrySyncPingBundleBuilder.UPLOAD_REASON_SCHEDULE;
        }

        // No reason to upload.
        return null;
    }

    // This has storage side-effects.
    private boolean setOrUpdateIDsIfChanged(@NonNull SharedPreferences prefs, @NonNull String uid, @NonNull String deviceID) {
        final String currentIDsCombined = uid.concat(deviceID);
        final String previousIDsHash = prefs.getString(PREF_IDS, "");

        // Persist IDs for the first time, declare them as "not changed".
        if (previousIDsHash.equals("")) {
            final SharedPreferences.Editor prefsEditor = prefs.edit();
            prefsEditor.putString(PREF_IDS, currentIDsCombined);
            prefsEditor.apply();
            return false;
        }

        // If IDs are different update local cache and declare them as "changed".
        if (!previousIDsHash.equals(currentIDsCombined)) {
            final SharedPreferences.Editor prefsEditor = prefs.edit();
            prefsEditor.putString(PREF_IDS, currentIDsCombined);
            prefsEditor.apply();
            return true;
        }

        // Nothing changed, and no side-effects took place.
        return false;
    }

}
