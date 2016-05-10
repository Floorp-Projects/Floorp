/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.telemetry;

import android.app.IntentService;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.util.Log;
import ch.boye.httpclientandroidlib.HttpHeaders;
import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.client.ClientProtocolException;
import ch.boye.httpclientandroidlib.client.methods.HttpRequestBase;
import ch.boye.httpclientandroidlib.impl.client.DefaultHttpClient;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.preferences.GeckoPreferences;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.net.BaseResource;
import org.mozilla.gecko.sync.net.BaseResourceDelegate;
import org.mozilla.gecko.sync.net.Resource;
import org.mozilla.gecko.telemetry.stores.TelemetryPingStore;
import org.mozilla.gecko.util.DateUtil;
import org.mozilla.gecko.util.NetworkUtils;
import org.mozilla.gecko.util.StringUtils;

import java.io.IOException;
import java.net.URISyntaxException;
import java.security.GeneralSecurityException;
import java.util.Calendar;
import java.util.HashSet;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * The service that handles retrieving a list of telemetry pings to upload from the given
 * {@link TelemetryPingStore}, uploading those payloads to the associated server, and reporting
 * back to the Store which uploads were a success.
 */
public class TelemetryUploadService extends IntentService {
    private static final String LOGTAG = StringUtils.safeSubstring("Gecko" + TelemetryUploadService.class.getSimpleName(), 0, 23);
    private static final String WORKER_THREAD_NAME = LOGTAG + "Worker";

    public static final String ACTION_UPLOAD = "upload";
    public static final String EXTRA_STORE = "store";

    public TelemetryUploadService() {
        super(WORKER_THREAD_NAME);

        // Intent redelivery can fail hard (e.g. we OOM as we try to upload, the Intent gets redelivered, repeat)
        // so for simplicity, we avoid it. We expect the upload service to eventually get called again by the caller.
        setIntentRedelivery(false);
    }

    /**
     * Handles a ping with the mandatory extras:
     *   * EXTRA_STORE: A {@link TelemetryPingStore} where the pings to upload are located
     */
    @Override
    public void onHandleIntent(final Intent intent) {
        Log.d(LOGTAG, "Service started");

        if (!isReadyToUpload(this, intent)) {
            return;
        }

        final TelemetryPingStore store = intent.getParcelableExtra(EXTRA_STORE);
        final boolean wereAllUploadsSuccessful = uploadPendingPingsFromStore(this, store);
        store.maybePrunePings();
        Log.d(LOGTAG, "Service finished: upload and prune attempts completed");

        if (!wereAllUploadsSuccessful) {
            // If we had an upload failure, we should stop the IntentService and drop any
            // pending Intents in the queue so we don't waste resources (e.g. battery)
            // trying to upload when there's likely to be another connection failure.
            Log.d(LOGTAG, "Clearing Intent queue due to connection failures");
            stopSelf();
        }
    }

    /**
     * @return true if all pings were uploaded successfully, false otherwise.
     */
    private static boolean uploadPendingPingsFromStore(final Context context, final TelemetryPingStore store) {
        final List<TelemetryPing> pingsToUpload = store.getAllPings();
        if (pingsToUpload.isEmpty()) {
            return true;
        }

        final String serverSchemeHostPort = getServerSchemeHostPort(context);
        final HashSet<Integer> successfulUploadIDs = new HashSet<>(pingsToUpload.size()); // used for side effects.
        final PingResultDelegate delegate = new PingResultDelegate(successfulUploadIDs);
        for (final TelemetryPing ping : pingsToUpload) {
            // TODO: It'd be great to re-use the same HTTP connection for each upload request.
            delegate.setPingID(ping.getUniqueID());
            final String url = serverSchemeHostPort + "/" + ping.getURLPath();
            uploadPayload(url, ping.getPayload(), delegate);

            // There are minimal gains in trying to upload if we already failed one attempt.
            if (delegate.hadConnectionError()) {
                break;
            }
        }

        final boolean wereAllUploadsSuccessful = !delegate.hadConnectionError();
        if (wereAllUploadsSuccessful) {
            // We don't log individual successful uploads to avoid log spam.
            Log.d(LOGTAG, "Telemetry upload success!");
        }
        store.onUploadAttemptComplete(successfulUploadIDs);
        return wereAllUploadsSuccessful;
    }

    private static String getServerSchemeHostPort(final Context context) {
        // TODO (bug 1241685): Sync this preference with the gecko preference or a Java-based pref.
        // We previously had this synced with profile prefs with no way to change it in the client, so
        // we don't have to worry about losing data by switching it to app prefs, which is more convenient for now.
        final SharedPreferences sharedPrefs = GeckoSharedPrefs.forApp(context);
        return sharedPrefs.getString(TelemetryConstants.PREF_SERVER_URL, TelemetryConstants.DEFAULT_SERVER_URL);
    }

    private static void uploadPayload(final String url, final ExtendedJSONObject payload, final ResultDelegate delegate) {
        final BaseResource resource;
        try {
            resource = new BaseResource(url);
        } catch (final URISyntaxException e) {
            Log.w(LOGTAG, "URISyntaxException for server URL when creating BaseResource: returning.");
            return;
        }

        delegate.setResource(resource);
        resource.delegate = delegate;
        resource.setShouldCompressUploadedEntity(true);
        resource.setShouldChunkUploadsHint(false); // Telemetry servers don't support chunking.

        // We're in a background thread so we don't have any reason to do this asynchronously.
        // If we tried, onStartCommand would return and IntentService might stop itself before we finish.
        resource.postBlocking(payload);
    }

    private static boolean isReadyToUpload(final Context context, final Intent intent) {
        // Sanity check: is upload enabled? Generally, the caller should check this before starting the service.
        // Since we don't have the profile here, we rely on the caller to check the enabled state for the profile.
        if (!isUploadEnabledByAppConfig(context)) {
            Log.w(LOGTAG, "Upload is not available by configuration; returning");
            return false;
        }

        if (!isIntentValid(intent)) {
            Log.w(LOGTAG, "Received invalid Intent; returning");
            return false;
        }

        if (!ACTION_UPLOAD.equals(intent.getAction())) {
            Log.w(LOGTAG, "Unknown action: " + intent.getAction() + ". Returning");
            return false;
        }

        return true;
    }

    /**
     * Determines if the telemetry upload feature is enabled via the application configuration. Prefer to use
     * {@link #isUploadEnabledByProfileConfig(Context, GeckoProfile)} if the profile is available as it takes into
     * account more information.
     *
     * Note that this method logs debug statements when upload is disabled.
     */
    public static boolean isUploadEnabledByAppConfig(final Context context) {
        if (!TelemetryConstants.UPLOAD_ENABLED) {
            Log.d(LOGTAG, "Telemetry upload feature is compile-time disabled");
            return false;
        }

        if (!GeckoPreferences.getBooleanPref(context, GeckoPreferences.PREFS_HEALTHREPORT_UPLOAD_ENABLED, true)) {
            Log.d(LOGTAG, "Telemetry upload opt-out");
            return false;
        }

        if (!NetworkUtils.isBackgroundDataEnabled(context)) {
            Log.d(LOGTAG, "Background data is disabled");
            return false;
        }

        return true;
    }

    /**
     * Determines if the telemetry upload feature is enabled via profile & application level configurations. This is the
     * preferred method.
     *
     * Note that this method logs debug statements when upload is disabled.
     */
    public static boolean isUploadEnabledByProfileConfig(final Context context, final GeckoProfile profile) {
        if (profile.inGuestMode()) {
            Log.d(LOGTAG, "Profile is in guest mode");
            return false;
        }

        return isUploadEnabledByAppConfig(context);
    }

    private static boolean isIntentValid(final Intent intent) {
        // Intent can be null. Bug 1025937.
        if (intent == null) {
            Log.d(LOGTAG, "Received null intent");
            return false;
        }

        if (intent.getParcelableExtra(EXTRA_STORE) == null) {
            Log.d(LOGTAG, "Received invalid store in Intent");
            return false;
        }

        return true;
    }

    /**
     * Logs on success & failure and appends the set ID to the given Set on success.
     *
     * Note: you *must* set the ping ID before attempting upload or we'll throw!
     *
     * We use mutation on the set ID and the successful upload array to avoid object allocation.
     */
    private static class PingResultDelegate extends ResultDelegate {
        // We persist pings and don't need to worry about losing data so we keep these
        // durations short to save resources (e.g. battery).
        private static final int SOCKET_TIMEOUT_MILLIS = (int) TimeUnit.SECONDS.toMillis(30);
        private static final int CONNECTION_TIMEOUT_MILLIS = (int) TimeUnit.SECONDS.toMillis(30);

        /** The store ID of the ping currently being uploaded. Use {@link #getPingID()} to access it. */
        private int pingID = -1;
        private final HashSet<Integer> successfulUploadIDs;

        private boolean hadConnectionError = false;

        public PingResultDelegate(final HashSet<Integer> successfulUploadIDs) {
            super();
            this.successfulUploadIDs = successfulUploadIDs;
        }

        @Override
        public int socketTimeout() {
            return SOCKET_TIMEOUT_MILLIS;
        }

        @Override
        public int connectionTimeout() {
            return CONNECTION_TIMEOUT_MILLIS;
        }

        private int getPingID() {
            if (pingID < 0) {
                throw new IllegalStateException("Expected ping ID to have been updated before retrieval");
            }
            return pingID;
        }

        public void setPingID(final int id) {
            pingID = id;
        }

        @Override
        public String getUserAgent() {
            return TelemetryConstants.USER_AGENT;
        }

        @Override
        public void handleHttpResponse(final HttpResponse response) {
            final int status = response.getStatusLine().getStatusCode();
            switch (status) {
                case 200:
                case 201:
                    successfulUploadIDs.add(getPingID());
                    break;
                default:
                    Log.w(LOGTAG, "Telemetry upload failure. HTTP status: " + status);
                    hadConnectionError = true;
            }
        }

        @Override
        public void handleHttpProtocolException(final ClientProtocolException e) {
            // We don't log the exception to prevent leaking user data.
            Log.w(LOGTAG, "HttpProtocolException when trying to upload telemetry");
            hadConnectionError = true;
        }

        @Override
        public void handleHttpIOException(final IOException e) {
            // We don't log the exception to prevent leaking user data.
            Log.w(LOGTAG, "HttpIOException when trying to upload telemetry");
            hadConnectionError = true;
        }

        @Override
        public void handleTransportException(final GeneralSecurityException e) {
            // We don't log the exception to prevent leaking user data.
            Log.w(LOGTAG, "Transport exception when trying to upload telemetry");
            hadConnectionError = true;
        }

        private boolean hadConnectionError() {
            return hadConnectionError;
        }

        @Override
        public void addHeaders(final HttpRequestBase request, final DefaultHttpClient client) {
            super.addHeaders(request, client);
            request.addHeader(HttpHeaders.DATE, DateUtil.getDateInHTTPFormat(Calendar.getInstance().getTime()));
        }
    }

    /**
     * A hack because I want to set the resource after the Delegate is constructed.
     * Be sure to call {@link #setResource(Resource)}!
     */
    private static abstract class ResultDelegate extends BaseResourceDelegate {
        public ResultDelegate() {
            super(null);
        }

        protected void setResource(final Resource resource) {
            this.resource = resource;
        }
    }
}
