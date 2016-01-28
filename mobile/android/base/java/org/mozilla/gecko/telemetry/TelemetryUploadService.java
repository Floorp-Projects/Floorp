/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.telemetry;

import android.content.Intent;
import android.content.SharedPreferences;
import android.support.annotation.NonNull;
import android.util.Log;
import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.client.ClientProtocolException;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.background.BackgroundService;
import org.mozilla.gecko.sync.net.BaseResource;
import org.mozilla.gecko.sync.net.BaseResourceDelegate;
import org.mozilla.gecko.sync.net.Resource;
import org.mozilla.gecko.util.StringUtils;

import java.io.IOException;
import java.net.URISyntaxException;
import java.security.GeneralSecurityException;

/**
 * The service that handles uploading telemetry payloads to the server.
 *
 * Note that we'll fail to upload if the network is off or background uploads are disabled but the caller is still
 * expected to increment the sequence number.
 */
public class TelemetryUploadService extends BackgroundService {
    private static final String LOGTAG = StringUtils.safeSubstring("Gecko" + TelemetryUploadService.class.getSimpleName(), 0, 23);
    private static final String WORKER_THREAD_NAME = LOGTAG + "Worker";

    public TelemetryUploadService() {
        super(WORKER_THREAD_NAME);

        // Intent redelivery can fail hard (e.g. we OOM as we try to upload, the Intent gets redelivered, repeat) so for
        // simplicity, we avoid it for now. In the unlikely event that Android kills our upload service, we'll thus fail
        // to upload the document with a specific sequence number. Furthermore, we never attempt to re-upload it.
        //
        // We'll fix this issue in bug 1243585.
        setIntentRedelivery(false);
    }

    /**
     * Handles a core ping with the mandatory extras:
     *   EXTRA_DOC_ID: a unique document ID.
     *   EXTRA_SEQ: a sequence number for this upload.
     *   EXTRA_PROFILE_NAME: the gecko profile name.
     *   EXTRA_PROFILE_PATH: the gecko profile path.
     *
     * Note that for a given doc ID, seq should always be identical because these are the tools the server uses to
     * de-duplicate documents. In order to maintain this consistency, we receive the doc ID and seq from the Intent and
     * rely on the caller to update the values. The Service can be killed at any time so we can't ensure seq could be
     * incremented properly if we tried to do so in the Service.
     */
    @Override
    public void onHandleIntent(final Intent intent) {
        Log.d(LOGTAG, "Service started");

        if (!TelemetryConstants.UPLOAD_ENABLED) {
            Log.d(LOGTAG, "Telemetry upload feature is compile-time disabled; not handling upload intent.");
            return;
        }

        if (!isReadyToUpload(intent)) {
            return;
        }

        if (!TelemetryConstants.ACTION_UPLOAD_CORE.equals(intent.getAction())) {
            Log.w(LOGTAG, "Unknown action: " + intent.getAction() + ". Returning");
            return;
        }

        final String docId = intent.getStringExtra(TelemetryConstants.EXTRA_DOC_ID);
        final int seq = intent.getIntExtra(TelemetryConstants.EXTRA_SEQ, -1);

        final String profileName = intent.getStringExtra(TelemetryConstants.EXTRA_PROFILE_NAME);
        final String profilePath = intent.getStringExtra(TelemetryConstants.EXTRA_PROFILE_PATH);

        uploadCorePing(docId, seq, profileName, profilePath);
    }

    private boolean isReadyToUpload(final Intent intent) {
        // Intent can be null. Bug 1025937.
        if (intent == null) {
            Log.d(LOGTAG, "Received null intent. Returning.");
            return false;
        }

        // Don't do anything if the device can't talk to the server.
        if (!backgroundDataIsEnabled()) {
            Log.d(LOGTAG, "Background data is not enabled; skipping.");
            return false;
        }

        if (intent.getStringExtra(TelemetryConstants.EXTRA_DOC_ID) == null) {
            Log.w(LOGTAG, "Received invalid doc ID in Intent. Returning");
            return false;
        }

        if (!intent.hasExtra(TelemetryConstants.EXTRA_SEQ)) {
            Log.w(LOGTAG, "Received Intent without sequence number. Returning");
            return false;
        }

        if (intent.getStringExtra(TelemetryConstants.EXTRA_PROFILE_NAME) == null) {
            Log.w(LOGTAG, "Received invalid profile name in Intent. Returning");
            return false;
        }

        // GeckoProfile can use the name to get the path so this isn't strictly necessary.
        // However, getting the path requires parsing an ini file so we optimize by including it here.
        if (intent.getStringExtra(TelemetryConstants.EXTRA_PROFILE_PATH) == null) {
            Log.w(LOGTAG, "Received invalid profile path in Intent. Returning");
            return false;
        }

        return true;
    }

    private void uploadCorePing(@NonNull final String docId, final int seq, @NonNull final String profileName,
                @NonNull final String profilePath) {
        final GeckoProfile profile = GeckoProfile.get(this, profileName, profilePath);

        final String clientId;
        try {
            clientId = profile.getClientId();
        } catch (final IOException e) {
            // Don't log the exception to avoid leaking the profile path.
            Log.w(LOGTAG, "Unable to get client ID to generate core ping: returning.");
            return;
        }

        // Each profile can have different telemetry data so we intentionally grab the shared prefs for the profile.
        final SharedPreferences sharedPrefs = GeckoSharedPrefs.forProfileName(this, profileName);
        // TODO (bug 1241685): Sync this preference with the gecko preference.
        final String serverURLSchemeHostPort =
                sharedPrefs.getString(TelemetryConstants.PREF_SERVER_URL, TelemetryConstants.DEFAULT_SERVER_URL);

        final TelemetryPing corePing =
                TelemetryPingGenerator.createCorePing(docId, clientId, serverURLSchemeHostPort, seq);
        final CorePingResultDelegate resultDelegate = new CorePingResultDelegate();
        uploadPing(corePing, resultDelegate);
    }

    private void uploadPing(final TelemetryPing ping, final ResultDelegate delegate) {
        final BaseResource resource;
        try {
            resource = new BaseResource(ping.getURL());
        } catch (final URISyntaxException e) {
            Log.w(LOGTAG, "URISyntaxException for server URL when creating BaseResource: returning.");
            return;
        }

        delegate.setResource(resource);
        resource.delegate = delegate;

        // We're in a background thread so we don't have any reason to do this asynchronously.
        // If we tried, onStartCommand would return and IntentService might stop itself before we finish.
        resource.postBlocking(ping.getPayload());
    }

    private static class CorePingResultDelegate extends ResultDelegate {
        public CorePingResultDelegate() {
            super();
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
                    Log.d(LOGTAG, "Telemetry upload success.");
                    break;
                default:
                    Log.w(LOGTAG, "Telemetry upload failure. HTTP status: " + status);
            }
        }

        @Override
        public void handleHttpProtocolException(final ClientProtocolException e) {
            // We don't log the exception to prevent leaking user data.
            Log.w(LOGTAG, "HttpProtocolException when trying to upload telemetry");
        }

        @Override
        public void handleHttpIOException(final IOException e) {
            // We don't log the exception to prevent leaking user data.
            Log.w(LOGTAG, "HttpIOException when trying to upload telemetry");
        }

        @Override
        public void handleTransportException(final GeneralSecurityException e) {
            // We don't log the exception to prevent leaking user data.
            Log.w(LOGTAG, "Transport exception when trying to upload telemetry");
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
