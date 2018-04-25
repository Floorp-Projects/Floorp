/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.config;

import android.content.Context;
import android.content.SharedPreferences;

import org.mozilla.telemetry.measurement.SettingsMeasurement;
import org.mozilla.telemetry.util.ContextUtils;

import java.io.File;
import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/**
 * The TelemetryConfiguration class collects the information describing the telemetry setup of an app.
 *
 * There are some parts that every app needs to configure: Where should measurements store data?
 * What servers are we actually uploading pings to? This class should provide good defaults so that
 * in the best case it is not needed to modify the configuration.
 */
public class TelemetryConfiguration {
    private static final String DEFAULT_DATA_DIRECTORY = "telemetry";
    private static final String DEFAULT_SHARED_PREFERENCE = "telemetry_preferences";
    private static final String DEFAULT_ENDPOINT = "https://incoming.telemetry.mozilla.org";
    private static final String DEFAULT_USER_AGENT = "Telemetry/1.0 (Android)";

    private static final long DEFAULT_INITAL_BACKOFF_FOR_UPLOAD = 30000;
    private static final int DEFAULT_CONNECT_TIMEOUT = 10000;
    private static final int DEFAULT_READ_TIMEOUT = 30000;
    private static final int DEFAULT_MINIMUM_EVENTS_FOR_UPLOAD = 3;
    private static final String DEFAULT_UPDATE_CHANNEL = "unknown";
    private static final int DEFAULT_MAXIMUM_NUMBER_OF_PINGS_PER_EVENT = 500;
    private static final int DEFAULT_MAXIMUM_PINGS_PER_TYPE = 40;
    private static final int DEFAULT_MAXIMUM_PING_UPLOADS_PER_DAY = 100;

    private static final long classLoadTimestampMillis = System.currentTimeMillis();

    private final Context context;

    private String appName;
    private String appVersion;
    private String buildId;
    private String updateChannel;
    private boolean collectionEnabled;
    private boolean uploadEnabled;
    private String serverEndpoint;
    private File dataDirectory;
    private Set<String> telemetryPreferences;
    private long initialBackoffForUpload;
    private int connectTimeout;
    private int readTimeout;
    private String userAgent;
    private int minimumEventsForUpload;
    private int maximumNumberOfEventsPerPing;
    private int maximumNumberOfPingsPerType;
    private int maximumNumberOfPingUploadsPerDay;
    private SettingsMeasurement.SettingsProvider settingsProvider;

    public TelemetryConfiguration(Context context) {
        this.context = context.getApplicationContext();
        this.telemetryPreferences = Collections.emptySet();

        setAppName(ContextUtils.getAppName(context));
        setAppVersion(ContextUtils.getVersionName(context));
        setBuildId(String.valueOf(ContextUtils.getVersionCode(context)));
        setUpdateChannel(DEFAULT_UPDATE_CHANNEL);
        setDataDirectory(new File(context.getApplicationInfo().dataDir, DEFAULT_DATA_DIRECTORY));
        setServerEndpoint(DEFAULT_ENDPOINT);
        setInitialBackoffForUpload(DEFAULT_INITAL_BACKOFF_FOR_UPLOAD);
        setConnectTimeout(DEFAULT_CONNECT_TIMEOUT);
        setReadTimeout(DEFAULT_READ_TIMEOUT);
        setUserAgent(DEFAULT_USER_AGENT);
        setMinimumEventsForUpload(DEFAULT_MINIMUM_EVENTS_FOR_UPLOAD);
        setCollectionEnabled(true);
        setUploadEnabled(true);
        setMaximumNumberOfEventsPerPing(DEFAULT_MAXIMUM_NUMBER_OF_PINGS_PER_EVENT);
        setMaximumNumberOfPingsPerType(DEFAULT_MAXIMUM_PINGS_PER_TYPE);
        setMaximumNumberOfPingUploadsPerDay(DEFAULT_MAXIMUM_PING_UPLOADS_PER_DAY);
        setSettingsProvider(new SettingsMeasurement.SharedPreferenceSettingsProvider());
    }

    /**
     * Application context this library is running in.
     */
    public Context getContext() {
        return context;
    }

    /**
     * Set the root directory where telemetry components should store data.
     */
    public TelemetryConfiguration setDataDirectory(File dataDirectory) {
        if (!dataDirectory.exists() && !dataDirectory.mkdirs()) {
            throw new IllegalStateException(
                    "Telemetry data directory does not exist and can't be created: " + dataDirectory.getAbsolutePath());
        }

        if (!dataDirectory.isDirectory() || !dataDirectory.canWrite()) {
            throw new IllegalStateException(
                    "Telemetry data directory is not writeable directory" + dataDirectory.getAbsolutePath());
        }

        this.dataDirectory = dataDirectory;

        return this;
    }

    /**
     * Get the root directory where telemetry components should store data.
     */
    public File getDataDirectory() {
        return dataDirectory;
    }

    /**
     * Set the server endpoint to upload telemetry pings to. And endpoint value should include the
     * schema and no trailing slash, e.g.: https://telemetry.example.org
     */
    public TelemetryConfiguration setServerEndpoint(String endpoint) {
        this.serverEndpoint = endpoint;
        return this;
    }

    /**
     * Set the server endpoint to upload telemetry pings to.
     */
    public String getServerEndpoint() {
        return serverEndpoint;
    }

    /**
     * Get shared preference for storing telemetry related data.
     */
    public SharedPreferences getSharedPreferences() {
        return context.getSharedPreferences(DEFAULT_SHARED_PREFERENCE, Context.MODE_PRIVATE);
    }

    /**
     * Set a list of preference keys that are important for telemetry. Some measurements and pings
     * might use this to determine what preferences should be reported.
     */
    public TelemetryConfiguration setPreferencesImportantForTelemetry(String... preferences) {
        Set<String> set = new HashSet<>();
        Collections.addAll(set, preferences);

        this.telemetryPreferences = set;

        return this;
    }

    /**
     * Get a list of preference keys that are important for telemetry.
     */
    public Set<String> getPreferencesImportantForTelemetry() {
        return telemetryPreferences;
    }

    /**
     * Get the interval (in milliseconds) to wait initially after a ping upload has failed.
     */
    public long getInitialBackoffForUpload() {
        return initialBackoffForUpload;
    }

    /**
     * Set the interval (in milliseconds) to wait initially after a ping upload has failed.
     */
    public TelemetryConfiguration setInitialBackoffForUpload(long initialBackoffForUpload) {
        this.initialBackoffForUpload = initialBackoffForUpload;
        return this;
    }

    /**
     * Returns setting for connect timeout (milliseconds). A value of 0 implies that the option is
     * disabled (i.e., timeout of infinity).
     */
    public int getConnectTimeout() {
        return connectTimeout;
    }

    /**
     * Returns setting for read timeout. 0 return implies that the option is disabled (i.e., timeout
     * of infinity).
     */
    public int getReadTimeout() {
        return readTimeout;
    }

    /**
     * Sets a specified timeout value, in milliseconds, to be used when opening a communications
     * link to the telemetry endpoint.
     *
     * If the timeout expires before the connection can be established, the ping upload will be
     * retried at a later time. A timeout of zero is interpreted as an infinite timeout.
     */
    public TelemetryConfiguration setConnectTimeout(int connectTimeout) {
        this.connectTimeout = connectTimeout;
        return this;
    }

    /**
     * Sets the read timeout to a specified timeout, in milliseconds. A non-zero value specifies the
     * timeout when reading from the telemetry endpoint. If the timeout expires before there is data
     * available for read, the ping upload will be retried later. A timeout of zero is interpreted as
     * an infinite timeout.
     */
    public TelemetryConfiguration setReadTimeout(int readTimeout) {
        this.readTimeout = readTimeout;
        return this;
    }

    /**
     * Get the user agent used when communicating with the telemetry endpoint.
     */
    public String getUserAgent() {
        return userAgent;
    }

    /**
     * Set the user agent used when communicating with the telemetry endpoint.
     */
    public TelemetryConfiguration setUserAgent(String userAgent) {
        this.userAgent = userAgent;
        return this;
    }

    /**
     * Get the minimum number of telemetry events that need to be fired before even trying to upload
     * an event ping.
     */
    public int getMinimumEventsForUpload() {
        return minimumEventsForUpload;
    }

    /**
     * Set the minimum number of telemetry events that need to be fired before even trying to upload
     * an event ping. The default value is 3. The minimum needs to be >= 1.
     */
    public TelemetryConfiguration setMinimumEventsForUpload(int minimumEventsForUpload) {
        if (minimumEventsForUpload <= 0) {
            throw new IllegalArgumentException("minimumEventsForUpload needs to be >= 1");
        }

        this.minimumEventsForUpload = minimumEventsForUpload;
        return this;
    }

    /**
     * Is collecting telemetry data enabled?
     */
    public boolean isCollectionEnabled() {
        return collectionEnabled;
    }

    /**
     * Enable or disable collection of telemetry data. Local dev builds should disable this to avoid
     * collecting and sending any data.
     */
    public TelemetryConfiguration setCollectionEnabled(boolean collectionEnabled) {
        this.collectionEnabled = collectionEnabled;
        return this;
    }

    /**
     * Enable or disable the upload of collected telemetry data. This is usually set based on a
     * user visible preference.
     */
    public TelemetryConfiguration setUploadEnabled(boolean uploadEnabled) {
        this.uploadEnabled = uploadEnabled;
        return this;
    }

    /**
     * Is uploading collected telemetry data enable?
     */
    public boolean isUploadEnabled() {
        return uploadEnabled;
    }

    /**
     * Get the name of the app. This value is used as one dimension when uploading telemetry to the
     * HTTP Edge server.
     */
    public String getAppName() {
        return appName;
    }

    /**
     * Set the name of the app. This value is used as one dimension when uploading telemetry to the
     * HTTP Edge server.
     */
    public TelemetryConfiguration setAppName(String appName) {
        this.appName = appName;
        return this;
    }

    /**
     * Set the update channel of the app (e.g. beta, release). This value is used as one dimension
     * when uploading telemetry to the HTTP Edge server.
     */
    public TelemetryConfiguration setUpdateChannel(String updateChannel) {
        this.updateChannel = updateChannel;
        return this;
    }

    /**
     * Get the update channel of the app (e.g. beta, release). This value is used as one dimension
     * when uploading telemetry to the HTTP Edge server.
     */
    public String getUpdateChannel() {
        return updateChannel;
    }

    /**
     * Get the version of the app (e.g. 45.0.1). This value is used as one dimension when uploading
     * telemetry to the HTTP Edge server.
     */
    public String getAppVersion() {
        return appVersion;
    }

    /**
     * Set the version of the app (e.g. 45.0.1). This value is used as one dimension when uploading
     * telemetry to the HTTP Edge server.
     */
    public TelemetryConfiguration setAppVersion(String appVersion) {
        this.appVersion = appVersion;
        return this;
    }

    /**
     * Get the build id of the app (e.g. 20150125030202). This value is used as one dimension when
     * uploading telemetry to the HTTP Edge server.
     */
    public String getBuildId() {
        return buildId;
    }

    /**
     * Get the build id of the app (e.g. 20150125030202). This value is used as one dimension when
     * uploading telemetry to the HTTP Edge server.
     */
    public TelemetryConfiguration setBuildId(String buildId) {
        this.buildId = buildId;
        return this;
    }

    /**
     * Set the maximum number of events per ping. If this limit is reached a ping will built and
     * stored automatically. The number of stored and uploaded pings might be limited too.
     */
    public TelemetryConfiguration setMaximumNumberOfEventsPerPing(int maximumNumberOfEventsPerPing) {
        this.maximumNumberOfEventsPerPing = maximumNumberOfEventsPerPing;
        return this;
    }

    /**
     * Get the maximum number of events per ping.
     */
    public int getMaximumNumberOfEventsPerPing() {
        return maximumNumberOfEventsPerPing;
    }

    /**
     * Set the maximum number of pings that will be stored for a given ping type. If more types
     * are in the local store then pings will be removed (oldest first). For this to happen the
     * maximum needs to be reached without any successful upload.
     */
    public TelemetryConfiguration setMaximumNumberOfPingsPerType(int maximumNumberOfPingsPerType) {
        this.maximumNumberOfPingsPerType = maximumNumberOfPingsPerType;
        return this;
    }

    /**
     * Get the maximum number of pings that will be stored (for upload).
     */
    public int getMaximumNumberOfPingsPerType() {
        return maximumNumberOfPingsPerType;
    }

    /**
     * Get the maximum number of pings that should be uploaded per day.
     */
    public int getMaximumNumberOfPingUploadsPerDay() {
        return maximumNumberOfPingUploadsPerDay;
    }

    /**
     * Set the maximum number of pings uploaded per day. This limit is enforced for every type. If
     * you have 2 ping types and set a limit of 100 pings per day then in total 200 pings per day
     * could be uploaded.
     */
    public TelemetryConfiguration setMaximumNumberOfPingUploadsPerDay(int maximumNumberOfPingUploadsPerDay) {
        this.maximumNumberOfPingUploadsPerDay = maximumNumberOfPingUploadsPerDay;
        return this;
    }

    /**
     * Get the provider for reading app settings.
     */
    public SettingsMeasurement.SettingsProvider getSettingsProvider() {
        return settingsProvider;
    }

    /**
     * Set a provider for reading app settings.
     */
    public TelemetryConfiguration setSettingsProvider(SettingsMeasurement.SettingsProvider settingsProvider) {
        this.settingsProvider = settingsProvider;
        return this;
    }

    public long getClassLoadTimestampMillis() {
        return classLoadTimestampMillis;
    }
}
