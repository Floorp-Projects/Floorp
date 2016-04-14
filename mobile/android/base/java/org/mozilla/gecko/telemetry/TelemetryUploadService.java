/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.telemetry;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.WorkerThread;
import android.util.Log;
import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.client.ClientProtocolException;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.background.BackgroundService;
import org.mozilla.gecko.distribution.DistributionStoreCallback;
import org.mozilla.gecko.preferences.GeckoPreferences;
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

    private static final int MILLIS_IN_DAY = 1000 * 60 * 60 * 24;

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

        // Sanity check: is upload enabled? Generally, the caller should check this before starting the service.
        // Since we don't have the profile here, we rely on the caller to check the enabled state for the profile.
        if (!isUploadEnabledByAppConfig(this)) {
            Log.w(LOGTAG, "Upload is not available by configuration; returning");
            return;
        }

        if (!isIntentValid(intent)) {
            Log.w(LOGTAG, "Received invalid Intent; returning");
            return;
        }

        if (!TelemetryConstants.ACTION_UPLOAD_CORE.equals(intent.getAction())) {
            Log.w(LOGTAG, "Unknown action: " + intent.getAction() + ". Returning");
            return;
        }

        final String defaultSearchEngine = intent.getStringExtra(TelemetryConstants.EXTRA_DEFAULT_SEARCH_ENGINE);
        final String docId = intent.getStringExtra(TelemetryConstants.EXTRA_DOC_ID);
        final int seq = intent.getIntExtra(TelemetryConstants.EXTRA_SEQ, -1);

        final String profileName = intent.getStringExtra(TelemetryConstants.EXTRA_PROFILE_NAME);
        final String profilePath = intent.getStringExtra(TelemetryConstants.EXTRA_PROFILE_PATH);

        uploadCorePing(docId, seq, profileName, profilePath, defaultSearchEngine);
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

        if (!backgroundDataIsEnabled(context)) {
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

    private boolean isIntentValid(final Intent intent) {
        // Intent can be null. Bug 1025937.
        if (intent == null) {
            Log.d(LOGTAG, "Received null intent");
            return false;
        }

        if (intent.getStringExtra(TelemetryConstants.EXTRA_DOC_ID) == null) {
            Log.d(LOGTAG, "Received invalid doc ID in Intent");
            return false;
        }

        if (!intent.hasExtra(TelemetryConstants.EXTRA_SEQ)) {
            Log.d(LOGTAG, "Received Intent without sequence number");
            return false;
        }

        if (intent.getStringExtra(TelemetryConstants.EXTRA_PROFILE_NAME) == null) {
            Log.d(LOGTAG, "Received invalid profile name in Intent");
            return false;
        }

        // GeckoProfile can use the name to get the path so this isn't strictly necessary.
        // However, getting the path requires parsing an ini file so we optimize by including it here.
        if (intent.getStringExtra(TelemetryConstants.EXTRA_PROFILE_PATH) == null) {
            Log.d(LOGTAG, "Received invalid profile path in Intent");
            return false;
        }

        return true;
    }

    @WorkerThread
    private void uploadCorePing(@NonNull final String docId, final int seq, @NonNull final String profileName,
                @NonNull final String profilePath, @Nullable final String defaultSearchEngine) {
        final GeckoProfile profile = GeckoProfile.get(this, profileName, profilePath);
        final long profileCreationDate = getProfileCreationDate(profile);
        final String clientId;
        try {
            clientId = profile.getClientId();
        } catch (final IOException e) {
            Log.w(LOGTAG, "Unable to get client ID to generate core ping: returning.", e);
            return;
        }

        // Each profile can have different telemetry data so we intentionally grab the shared prefs for the profile.
        final SharedPreferences sharedPrefs = GeckoSharedPrefs.forProfileName(this, profileName);
        // TODO (bug 1241685): Sync this preference with the gecko preference.
        final String serverURLSchemeHostPort =
                sharedPrefs.getString(TelemetryConstants.PREF_SERVER_URL, TelemetryConstants.DEFAULT_SERVER_URL);
        final String distributionId = sharedPrefs.getString(DistributionStoreCallback.PREF_DISTRIBUTION_ID, null);
        final TelemetryPing corePing = TelemetryPingGenerator.createCorePing(this, docId, clientId,
                serverURLSchemeHostPort, seq, profileCreationDate, distributionId, defaultSearchEngine);
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
        resource.setShouldCompressUploadedEntity(true);
        resource.setShouldChunkUploadsHint(false); // Telemetry servers don't support chunking.

        // We're in a background thread so we don't have any reason to do this asynchronously.
        // If we tried, onStartCommand would return and IntentService might stop itself before we finish.
        resource.postBlocking(ping.getPayload());
    }

    /**
     * @return the profile creation date in the format expected by TelemetryPingGenerator.
     */
    @WorkerThread
    private long getProfileCreationDate(final GeckoProfile profile) {
        final long profileMillis = profile.getAndPersistProfileCreationDate(this);
        // getAndPersistProfileCreationDate can return -1,
        // and we don't want to truncate (-1 / MILLIS) to 0.
        if (profileMillis < 0) {
            return profileMillis;
        }
        return (long) Math.floor((double) profileMillis / MILLIS_IN_DAY);
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
