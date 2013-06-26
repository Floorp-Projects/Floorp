/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.health;

import java.util.ArrayList;

import android.content.Context;
import android.content.ContentProviderClient;
import android.content.SharedPreferences;
import android.util.Log;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.PrefsHelper;
import org.mozilla.gecko.PrefsHelper.PrefHandler;

import org.mozilla.gecko.background.healthreport.EnvironmentBuilder;
import org.mozilla.gecko.background.healthreport.HealthReportDatabaseStorage;
import org.mozilla.gecko.background.healthreport.HealthReportStorage.Field;
import org.mozilla.gecko.background.healthreport.HealthReportStorage.MeasurementFields;
import org.mozilla.gecko.background.healthreport.HealthReportStorage.MeasurementFields.FieldSpec;
import org.mozilla.gecko.background.healthreport.ProfileInformationCache;

import org.mozilla.gecko.util.EventDispatcher;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.ThreadUtils;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStreamWriter;
import java.nio.charset.Charset;
import java.util.ArrayList;
import java.util.Scanner;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * BrowserHealthRecorder is the browser's interface to the Firefox Health
 * Report storage system. It manages environments (a collection of attributes
 * that are tracked longitudinally) on the browser's behalf, exposing a simpler
 * interface for recording changes.
 *
 * Keep an instance of this class around.
 *
 * Tell it when an environment attribute has changed: call {@link
 * #onBlocklistPrefChanged(boolean)} or {@link
 * #onTelemetryPrefChanged(boolean)}, followed by {@link
 * #onEnvironmentChanged()}.
 *
 * Use it to record events: {@link #recordSearch(String, String)}.
 *
 * Shut it down when you're done being a browser: {@link #close()}.
 */
public class BrowserHealthRecorder implements GeckoEventListener {
    private static final String LOG_TAG = "GeckoHealthRec";
    private static final String PREF_BLOCKLIST_ENABLED = "extensions.blocklist.enabled";
    private static final String EVENT_ADDONS_ALL = "Addons:All";
    private static final String EVENT_ADDONS_CHANGE = "Addons:Change";
    private static final String EVENT_PREF_CHANGE = "Pref:Change";
 
    // This is raised from Gecko. It avoids browser.js having to know about the
    // location that invoked it (the URL bar).
    public static final String EVENT_KEYWORD_SEARCH = "Search:Keyword";

    // This is raised from Java. We include the location in the message.
    public static final String EVENT_SEARCH = "Search:Event";

    public enum State {
        NOT_INITIALIZED,
        INITIALIZING,
        INITIALIZED,
        INITIALIZATION_FAILED,
        CLOSED
    }

    protected volatile State state = State.NOT_INITIALIZED;

    private final AtomicBoolean orphanChecked = new AtomicBoolean(false);
    private volatile int env = -1;

    private ContentProviderClient client;
    private volatile HealthReportDatabaseStorage storage;
    private final ProfileInformationCache profileCache;
    private final EventDispatcher dispatcher;

    public static class SessionInformation {
        private static final String LOG_TAG = "GeckoSessInfo";

        public static final String PREFS_SESSION_START = "sessionStart";

        public final long wallStartTime;    // System wall clock.
        public final long realStartTime;    // Realtime clock.

        private final boolean wasOOM;
        private final boolean wasStopped;

        private volatile long timedGeckoStartup = -1;
        private volatile long timedJavaStartup = -1;

        // Current sessions don't (right now) care about wasOOM/wasStopped.
        // Eventually we might want to lift that logic out of GeckoApp.
        public SessionInformation(long wallTime, long realTime) {
            this(wallTime, realTime, false, false);
        }

        // Previous sessions do...
        public SessionInformation(long wallTime, long realTime, boolean wasOOM, boolean wasStopped) {
            this.wallStartTime = wallTime;
            this.realStartTime = realTime;
            this.wasOOM = wasOOM;
            this.wasStopped = wasStopped;
        }

        /**
         * Initialize a new SessionInformation instance from the supplied prefs object.
         *
         * This includes retrieving OOM/crash data, as well as timings.
         *
         * If no wallStartTime was found, that implies that the previous
         * session was correctly recorded, and an object with a zero
         * wallStartTime is returned.
         */
        public static SessionInformation fromSharedPrefs(SharedPreferences prefs) {
            boolean wasOOM = prefs.getBoolean(GeckoApp.PREFS_OOM_EXCEPTION, false);
            boolean wasStopped = prefs.getBoolean(GeckoApp.PREFS_WAS_STOPPED, true);
            long wallStartTime = prefs.getLong(PREFS_SESSION_START, 0L);
            long realStartTime = 0L;
            Log.d(LOG_TAG, "Building SessionInformation from prefs: " +
                           wallStartTime + ", " + realStartTime + ", " +
                           wasStopped + ", " + wasOOM);
            return new SessionInformation(wallStartTime, realStartTime, wasOOM, wasStopped);
        }

        public boolean wasKilled() {
            return wasOOM || !wasStopped;
        }

        /**
         * Record the beginning of this session to SharedPreferences by
         * recording our start time. If a session was already recorded, it is
         * overwritten (there can only be one running session at a time). Does
         * not commit the editor.
         */
        public void recordBegin(SharedPreferences.Editor editor) {
            Log.d(LOG_TAG, "Recording start of session: " + this.wallStartTime);
            editor.putLong(PREFS_SESSION_START, this.wallStartTime);
        }

        /**
         * Record the completion of this session to SharedPreferences by
         * deleting our start time. Does not commit the editor.
         */
        public void recordCompletion(SharedPreferences.Editor editor) {
            Log.d(LOG_TAG, "Recording session done: " + this.wallStartTime);
            editor.remove(PREFS_SESSION_START);
        }

        /**
         * Return the JSON that we'll put in the DB for this session.
         */
        public JSONObject getCompletionJSON(String reason, long realEndTime) throws JSONException {
            long durationSecs = (realEndTime - this.realStartTime) / 1000;
            JSONObject out = new JSONObject();
            out.put("r", reason);
            out.put("d", durationSecs);
            if (this.timedGeckoStartup > 0) {
                out.put("sg", this.timedGeckoStartup);
            }
            if (this.timedJavaStartup > 0) {
                out.put("sj", this.timedJavaStartup);
            }
            return out;
        }

        public JSONObject getCrashedJSON() throws JSONException {
            JSONObject out = new JSONObject();
            // We use ints here instead of booleans, because we're packing
            // stuff into JSON, and saving bytes in the DB is a worthwhile
            // goal.
            out.put("oom", this.wasOOM ? 1 : 0);
            out.put("stopped", this.wasStopped ? 1 : 0);
            out.put("r", "A");
            return out;
        }
    }

    // We track previousSession to avoid order-of-initialization confusion. We
    // accept it in the constructor, and process it after init.
    private final SessionInformation previousSession;
    private volatile SessionInformation session = null;
    public SessionInformation getCurrentSession() {
        return this.session;
    }

    public void setCurrentSession(SessionInformation session) {
        this.session = session;
    }

    public void recordGeckoStartupTime(long duration) {
        if (this.session == null) {
            return;
        }
        this.session.timedGeckoStartup = duration;
    }
    public void recordJavaStartupTime(long duration) {
        if (this.session == null) {
            return;
        }
        this.session.timedJavaStartup = duration;
    }

    /**
     * Persist the opaque identifier for the current Firefox Health Report environment.
     * This changes in certain circumstances; be sure to use the current value when recording data.
     */
    private void setHealthEnvironment(final int env) {
        this.env = env;
    }

    /**
     * This constructor does IO. Run it on a background thread.
     */
    public BrowserHealthRecorder(final Context context, final String profilePath, final EventDispatcher dispatcher, SessionInformation previousSession) {
        Log.d(LOG_TAG, "Initializing. Dispatcher is " + dispatcher);
        this.dispatcher = dispatcher;
        this.previousSession = previousSession;

        this.client = EnvironmentBuilder.getContentProviderClient(context);
        if (this.client == null) {
            throw new IllegalStateException("Could not fetch Health Report content provider.");
        }

        this.storage = EnvironmentBuilder.getStorage(this.client, profilePath);
        if (this.storage == null) {
            // Stick around even if we don't have storage: eventually we'll
            // want to report total failures of FHR storage itself, and this
            // way callers don't need to worry about whether their health
            // recorder didn't initialize.
            this.client.release();
            this.client = null;
        }

        this.profileCache = new ProfileInformationCache(profilePath);
        try {
            this.initialize(context, profilePath);
        } catch (Exception e) {
            Log.e(LOG_TAG, "Exception initializing.", e);
        }
    }

    /**
     * Shut down database connections, unregister event listeners, and perform
     * provider-specific uninitialization.
     */
    public synchronized void close() {
        switch (this.state) {
            case CLOSED:
                Log.w(LOG_TAG, "Ignoring attempt to double-close closed BrowserHealthRecorder.");
                return;
            case INITIALIZED:
                Log.i(LOG_TAG, "Closing Health Report client.");
                break;
            default:
                Log.i(LOG_TAG, "Closing incompletely initialized BrowserHealthRecorder.");
        }

        this.state = State.CLOSED;
        this.unregisterEventListeners();

        // Add any necessary provider uninitialization here.
        this.storage = null;
        if (this.client != null) {
            this.client.release();
            this.client = null;
        }
    }

    private void unregisterEventListeners() {
        this.dispatcher.unregisterEventListener(EVENT_ADDONS_ALL, this);
        this.dispatcher.unregisterEventListener(EVENT_ADDONS_CHANGE, this);
        this.dispatcher.unregisterEventListener(EVENT_PREF_CHANGE, this);
        this.dispatcher.unregisterEventListener(EVENT_KEYWORD_SEARCH, this);
        this.dispatcher.unregisterEventListener(EVENT_SEARCH, this);
    }

    public void onBlocklistPrefChanged(boolean to) {
        this.profileCache.beginInitialization();
        this.profileCache.setBlocklistEnabled(to);
    }

    public void onTelemetryPrefChanged(boolean to) {
        this.profileCache.beginInitialization();
        this.profileCache.setTelemetryEnabled(to);
    }

    public void onAddonChanged(String id, JSONObject json) {
        this.profileCache.beginInitialization();
        try {
            this.profileCache.updateJSONForAddon(id, json);
        } catch (IllegalStateException e) {
            Log.w(LOG_TAG, "Attempted to update add-on cache prior to full init.", e);
        }
    }

    /**
     * Call this when a material change has occurred in the running environment,
     * such that a new environment should be computed and prepared for use in
     * future events.
     *
     * Invoke this method after calls that mutate the environment, such as
     * {@link #onBlocklistPrefChanged(boolean)}.
     *
     * TODO: record a session transition with the new environment.
     */
    public synchronized void onEnvironmentChanged() {
        this.env = -1;
        try {
            profileCache.completeInitialization();
        } catch (java.io.IOException e) {
            Log.e(LOG_TAG, "Error completing profile cache initialization.", e);
            this.state = State.INITIALIZATION_FAILED;
            return;
        }
        ensureEnvironment();
    }

    protected synchronized int ensureEnvironment() {
        if (!(state == State.INITIALIZING ||
              state == State.INITIALIZED)) {
            throw new IllegalStateException("Not initialized.");
        }

        if (this.env != -1) {
            return this.env;
        }
        if (this.storage == null) {
            // Oh well.
            return -1;
        }
        return this.env = EnvironmentBuilder.registerCurrentEnvironment(this.storage,
                                                                        this.profileCache);
    }

    private static final String getTimesPath(final String profilePath) {
        return profilePath + File.separator + "times.json";
    }

    /**
     * Retrieve the stored profile creation time from the profile directory.
     *
     * @return the <code>created</code> value from the times.json file, or -1 on failure.
     */
    protected static long getProfileInitTimeFromFile(final String profilePath) {
        final File times = new File(getTimesPath(profilePath));
        Log.d(LOG_TAG, "Looking for " + times.getAbsolutePath());
        if (!times.exists()) {
            return -1;
        }

        Log.d(LOG_TAG, "Using times.json for profile creation time.");
        Scanner scanner = null;
        try {
            scanner = new Scanner(times, "UTF-8");
            final String contents = scanner.useDelimiter("\\A").next();
            return new JSONObject(contents).getLong("created");
        } catch (Exception e) {
            // There are assorted reasons why this might occur, but we
            // don't care. Move on.
            Log.w(LOG_TAG, "Failed to read times.json.", e);
        } finally {
            if (scanner != null) {
                scanner.close();
            }
        }
        return -1;
    }

    /**
     * Only works on API 9 and up.
     *
     * @return the package install time, or -1 if an error occurred.
     */
    protected static long getPackageInstallTime(final Context context) {
        if (android.os.Build.VERSION.SDK_INT < android.os.Build.VERSION_CODES.GINGERBREAD) {
            return -1;
        }

        try {
            return context.getPackageManager().getPackageInfo(AppConstants.ANDROID_PACKAGE_NAME, 0).firstInstallTime;
        } catch (android.content.pm.PackageManager.NameNotFoundException e) {
            Log.e(LOG_TAG, "Unable to fetch our own package info. This should never occur.", e);
        }
        return -1;
    }

    private static long getProfileInitTimeHeuristic(final Context context, final String profilePath) {
        // As a pretty good shortcut, settle for installation time.
        // In all recent Firefox profiles, times.json should exist.
        final long time = getPackageInstallTime(context);
        if (time != -1) {
            return time;
        }

        // Otherwise, fall back to the filesystem.
        // We'll settle for the modification time of the profile directory.
        Log.d(LOG_TAG, "Using profile directory modified time as proxy for profile creation time.");
        return new File(profilePath).lastModified();
    }

    private static long getAndPersistProfileInitTime(final Context context, final String profilePath) {
        // Let's look in the profile.
        long time = getProfileInitTimeFromFile(profilePath);
        if (time > 0) {
            Log.d(LOG_TAG, "Incorporating environment: times.json profile creation = " + time);
            return time;
        }

        // Otherwise, we need to compute a valid creation time and write it out.
        time = getProfileInitTimeHeuristic(context, profilePath);

        if (time > 0) {
            // Write out a stub times.json.
            try {
                FileOutputStream stream = new FileOutputStream(getTimesPath(profilePath));
                OutputStreamWriter writer = new OutputStreamWriter(stream, Charset.forName("UTF-8"));
                try {
                    writer.append("{\"created\": " + time + "}\n");
                } finally {
                    writer.close();
                }
            } catch (Exception e) {
                // Best-effort.
                Log.w(LOG_TAG, "Couldn't write times.json.", e);
            }
        }

        Log.d(LOG_TAG, "Incorporating environment: profile creation = " + time);
        return time;
    }

    private void handlePrefValue(final String pref, final boolean value) {
        Log.d(LOG_TAG, "Incorporating environment: " + pref + " = " + value);
        if (AppConstants.TELEMETRY_PREF_NAME.equals(pref)) {
            profileCache.setTelemetryEnabled(value);
            return;
        }
        if (PREF_BLOCKLIST_ENABLED.equals(pref)) {
            profileCache.setBlocklistEnabled(value);
            return;
        }
        Log.w(LOG_TAG, "Unexpected pref: " + pref);
    }

    /**
     * Background init helper.
     */
    private void initializeStorage() {
        Log.d(LOG_TAG, "Done initializing profile cache. Beginning storage init.");

        final BrowserHealthRecorder self = this;
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                synchronized (self) {
                    if (state != State.INITIALIZING) {
                        Log.w(LOG_TAG, "Unexpected state during init: " + state);
                        return;
                    }

                    // Belt and braces.
                    if (storage == null) {
                        Log.w(LOG_TAG, "Storage is null during init; shutting down?");
                        if (state == State.INITIALIZING) {
                            state = State.INITIALIZATION_FAILED;
                        }
                        return;
                    }

                    try {
                        storage.beginInitialization();
                    } catch (Exception e) {
                        Log.e(LOG_TAG, "Failed to init storage.", e);
                        state = State.INITIALIZATION_FAILED;
                        return;
                    }

                    try {
                        // Listen for add-ons and prefs changes.
                        dispatcher.registerEventListener(EVENT_ADDONS_CHANGE, self);
                        dispatcher.registerEventListener(EVENT_PREF_CHANGE, self);

                        // Initialize each provider here.
                        initializeSessionsProvider();
                        initializeSearchProvider();

                        Log.d(LOG_TAG, "Ensuring environment.");
                        ensureEnvironment();

                        Log.d(LOG_TAG, "Finishing init.");
                        storage.finishInitialization();
                        state = State.INITIALIZED;
                    } catch (Exception e) {
                        state = State.INITIALIZATION_FAILED;
                        storage.abortInitialization();
                        Log.e(LOG_TAG, "Initialization failed.", e);
                        return;
                    }

                    // Now do whatever we do after we start up.
                    checkForOrphanSessions();
                }
            }
        });
    }

    /**
     * Add provider-specific initialization in this method.
     */
    private synchronized void initialize(final Context context,
                                         final String profilePath)
        throws java.io.IOException {

        Log.d(LOG_TAG, "Initializing profile cache.");
        this.state = State.INITIALIZING;

        // If we can restore state from last time, great.
        if (this.profileCache.restoreUnlessInitialized()) {
            Log.d(LOG_TAG, "Successfully restored state. Initializing storage.");
            initializeStorage();
            return;
        }

        // Otherwise, let's initialize it from scratch.
        this.profileCache.beginInitialization();
        this.profileCache.setProfileCreationTime(getAndPersistProfileInitTime(context, profilePath));

        final BrowserHealthRecorder self = this;

        PrefHandler handler = new PrefsHelper.PrefHandlerBase() {
            @Override
            public void prefValue(String pref, boolean value) {
                handlePrefValue(pref, value);
            }

            @Override
            public void finish() {
                Log.d(LOG_TAG, "Requesting all add-ons from Gecko.");
                dispatcher.registerEventListener(EVENT_ADDONS_ALL, self);
                GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Addons:FetchAll", null));
                // Wait for the broadcast event which completes our initialization.
            }
        };

        // Oh, singletons.
        PrefsHelper.getPrefs(new String[] {
                                 AppConstants.TELEMETRY_PREF_NAME,
                                 PREF_BLOCKLIST_ENABLED
                             },
                             handler);
        Log.d(LOG_TAG, "Requested prefs.");
    }

    @Override
    public void handleMessage(String event, JSONObject message) {
        try {
            if (EVENT_ADDONS_ALL.equals(event)) {
                Log.d(LOG_TAG, "Got all add-ons.");
                try {
                    JSONObject addons = message.getJSONObject("json");
                    Log.i(LOG_TAG, "Persisting " + addons.length() + " add-ons.");
                    profileCache.setJSONForAddons(addons);
                    profileCache.completeInitialization();
                } catch (java.io.IOException e) {
                    Log.e(LOG_TAG, "Error completing profile cache initialization.", e);
                    state = State.INITIALIZATION_FAILED;
                    return;
                }

                if (state == State.INITIALIZING) {
                    initializeStorage();
                } else {
                    this.onEnvironmentChanged();
                }

                return;
            }
            if (EVENT_ADDONS_CHANGE.equals(event)) {
                Log.d(LOG_TAG, "Add-on changed: " + message.getString("id"));
                this.onAddonChanged(message.getString("id"), message.getJSONObject("json"));
                this.onEnvironmentChanged();
                return;
            }
            if (EVENT_PREF_CHANGE.equals(event)) {
                final String pref = message.getString("pref");
                Log.d(LOG_TAG, "Pref changed: " + pref);
                handlePrefValue(pref, message.getBoolean("value"));
                this.onEnvironmentChanged();
                return;
            }

            // Searches.
            if (EVENT_KEYWORD_SEARCH.equals(event)) {
                recordSearch(message.getString("identifier"), "bartext");
                return;
            }
            if (EVENT_SEARCH.equals(event)) {
                if (!message.has("location")) {
                    Log.d(LOG_TAG, "Ignoring search without location.");
                    return;
                }
                recordSearch(message.getString("identifier"), message.getString("location"));
                return;
            }
        } catch (Exception e) {
            Log.e(LOG_TAG, "Exception handling message \"" + event + "\":", e);
        }
    }

    /*
     * Searches.
     */

    public static final String MEASUREMENT_NAME_SEARCH_COUNTS = "org.mozilla.searches.counts";
    public static final int MEASUREMENT_VERSION_SEARCH_COUNTS = 4;

    public static final String[] SEARCH_LOCATIONS = {
        "barkeyword",
        "barsuggest",
        "bartext",
    };

    // See services/healthreport/providers.jsm. Sorry for the duplication.
    // THIS LIST MUST BE SORTED per java.lang.Comparable<String>.
    private static final String[] SEARCH_PROVIDERS = {
        "amazon-co-uk",
        "amazon-de",
        "amazon-en-GB",
        "amazon-france",
        "amazon-it",
        "amazon-jp",
        "amazondotcn",
        "amazondotcom",
        "amazondotcom-de",

        "aol-en-GB",
        "aol-web-search",

        "bing",

        "eBay",
        "eBay-de",
        "eBay-en-GB",
        "eBay-es",
        "eBay-fi",
        "eBay-france",
        "eBay-hu",
        "eBay-in",
        "eBay-it",

        "google",
        "google-jp",
        "google-ku",
        "google-maps-zh-TW",

        "mailru",

        "mercadolibre-ar",
        "mercadolibre-cl",
        "mercadolibre-mx",

        "seznam-cz",

        "twitter",
        "twitter-de",
        "twitter-ja",

        "wikipedia",            // Manually added.

        "yahoo",
        "yahoo-NO",
        "yahoo-answer-zh-TW",
        "yahoo-ar",
        "yahoo-bid-zh-TW",
        "yahoo-br",
        "yahoo-ch",
        "yahoo-cl",
        "yahoo-de",
        "yahoo-en-GB",
        "yahoo-es",
        "yahoo-fi",
        "yahoo-france",
        "yahoo-fy-NL",
        "yahoo-id",
        "yahoo-in",
        "yahoo-it",
        "yahoo-jp",
        "yahoo-jp-auctions",
        "yahoo-mx",
        "yahoo-sv-SE",
        "yahoo-zh-TW",

        "yandex",
        "yandex-ru",
        "yandex-slovari",
        "yandex-tr",
        "yandex.by",
        "yandex.ru-be",
    };

    private void initializeSearchProvider() {
        this.storage.ensureMeasurementInitialized(
            MEASUREMENT_NAME_SEARCH_COUNTS,
            MEASUREMENT_VERSION_SEARCH_COUNTS,
            new MeasurementFields() {
                @Override
                public Iterable<FieldSpec> getFields() {
                    ArrayList<FieldSpec> out = new ArrayList<FieldSpec>(SEARCH_LOCATIONS.length);
                    for (String location : SEARCH_LOCATIONS) {
                        // We're not using a counter, because the set of engine
                        // identifiers is potentially unbounded, and thus our
                        // measurement version would have to keep growing as
                        // fields changed. Instead we store discrete values, and
                        // accumulate them into a counting map during processing.
                        out.add(new FieldSpec(location, Field.TYPE_COUNTED_STRING_DISCRETE));
                    }
                    return out;
                }
        });

        // Do this here, rather than in a centralized registration spot, in
        // case the above throws and we wind up handling events that we can't
        // store.
        this.dispatcher.registerEventListener(EVENT_KEYWORD_SEARCH, this);
        this.dispatcher.registerEventListener(EVENT_SEARCH, this);
    }

    /**
     * Return the field key for the search provider. This turns null and
     * non-partner providers into "other".
     *
     * @param engine an engine identifier, such as "yandex"
     * @return the key to use, such as "other" or "yandex".
     */
    protected String getEngineKey(final String engine) {
        if (engine == null) {
            return "other";
        }

        // This is inefficient. Optimize if necessary.
        boolean found = (0 <= java.util.Arrays.binarySearch(SEARCH_PROVIDERS, engine));
        return found ? engine : "other";
    }

    /**
     * Record a search.
     *
     * @param engine the string identifier for the engine, or null if it's not a partner.
     * @param location one of a fixed set of locations: see {@link #SEARCH_LOCATIONS}.
     */
    public void recordSearch(final String engine, final String location) {
        if (this.state != State.INITIALIZED) {
            Log.d(LOG_TAG, "Not initialized: not recording search. (" + this.state + ")");
            return;
        }

        if (location == null) {
            throw new IllegalArgumentException("location must be provided for search.");
        }

        final int day = storage.getDay();
        final int env = this.env;
        final String key = getEngineKey(engine);
        final BrowserHealthRecorder self = this;

        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                final HealthReportDatabaseStorage storage = self.storage;
                if (storage == null) {
                    Log.d(LOG_TAG, "No storage: not recording search. Shutting down?");
                    return;
                }

                Log.d(LOG_TAG, "Recording search: " + key + ", " + location +
                               " (" + day + ", " + env + ").");
                final int searchField = storage.getField(MEASUREMENT_NAME_SEARCH_COUNTS,
                                                         MEASUREMENT_VERSION_SEARCH_COUNTS,
                                                         location)
                                               .getID();
                storage.recordDailyDiscrete(env, day, searchField, key);
            }
        });
    }

    /*
     * Sessions.
     *
     * We record session beginnings in SharedPreferences, because it's cheaper
     * to do that than to either write to then update the DB (which requires
     * keeping a row identifier to update, as well as two writes) or to record
     * two events (which doubles storage space and requires rollup logic).
     *
     * The pattern is:
     *
     * 1. On startup, determine whether an orphan session exists by looking for
     *    a saved timestamp in prefs. If it does, then record the orphan in FHR
     *    storage.
     * 2. Record in prefs that a new session has begun. Track the timestamp (so
     *    we know to which day the session belongs).
     * 3. As startup timings become available, accumulate them in memory.
     * 4. On clean shutdown, read the values from here, write them to the DB, and
     *    delete the sentinel time from SharedPreferences.
     * 5. On a dirty shutdown, the in-memory session will not be written to the
     *    DB, and the current session will be orphaned.
     *
     * Sessions are begun in onResume (and thus implicitly onStart) and ended
     * in onPause.
     *
     * Session objects are stored as discrete JSON.
     *
     *   "org.mozilla.appSessions": {
     *     _v: 4,
     *     "normal": [
     *       {"r":"P", "d": 123},
     *     ],
     *     "abnormal": [
     *       {"r":"A", "oom": true, "stopped": false}
     *     ]
     *   }
     *
     * "r": reason. Values are "P" (activity paused), "A" (abnormal termination)
     * "d": duration. Value in seconds.
     * "sg": Gecko startup time. Present if this is a clean launch.
     * "sj": Java startup time. Present if this is a clean launch.
     *
     * Abnormal terminations will be missing a duration and will feature these keys:
     *
     * "oom": was the session killed by an OOM exception?
     * "stopped": was the session stopped gently?
     */

    public static final String MEASUREMENT_NAME_SESSIONS = "org.mozilla.appSessions";
    public static final int MEASUREMENT_VERSION_SESSIONS = 4;

    private void initializeSessionsProvider() {
        this.storage.ensureMeasurementInitialized(
            MEASUREMENT_NAME_SESSIONS,
            MEASUREMENT_VERSION_SESSIONS,
            new MeasurementFields() {
                @Override
                public Iterable<FieldSpec> getFields() {
                    ArrayList<FieldSpec> out = new ArrayList<FieldSpec>(2);
                    out.add(new FieldSpec("normal", Field.TYPE_JSON_DISCRETE));
                    out.add(new FieldSpec("abnormal", Field.TYPE_JSON_DISCRETE));
                    return out;
                }
        });
    }

    /**
     * Logic shared between crashed and normal sessions.
     */
    private void recordSessionEntry(String field, SessionInformation session, JSONObject value) {
        try {
            final int sessionField = storage.getField(MEASUREMENT_NAME_SESSIONS,
                                                      MEASUREMENT_VERSION_SESSIONS,
                                                      field)
                                            .getID();
            final int day = storage.getDay(session.wallStartTime);
            storage.recordDailyDiscrete(env, day, sessionField, value);
        } catch (Exception e) {
            Log.w(LOG_TAG, "Unable to record session completion.", e);
        }
    }

    public void checkForOrphanSessions() {
        if (!this.orphanChecked.compareAndSet(false, true)) {
            Log.w(LOG_TAG, "Attempting to check for orphan sessions more than once.");
            return;
        }

        Log.d(LOG_TAG, "Checking for orphan session.");
        if (this.previousSession == null) {
            return;
        }
        if (this.previousSession.wallStartTime == 0) {
            return;
        }

        if (state != State.INITIALIZED) {
            // Something has gone awry.
            Log.e(LOG_TAG, "Attempted to record bad session end without initialized recorder.");
            return;
        }

        try {
            recordSessionEntry("abnormal", this.previousSession, this.previousSession.getCrashedJSON());
        } catch (Exception e) {
            Log.w(LOG_TAG, "Unable to generate session JSON.", e);

            // Future: record this exception in FHR's own error submitter.
        }
    }

    /**
     * Record that the current session ended. Does not commit the provided editor.
     */
    public void recordSessionEnd(String reason, SharedPreferences.Editor editor) {
        Log.d(LOG_TAG, "Recording session end: " + reason);
        if (state != State.INITIALIZED) {
            // Something has gone awry.
            Log.e(LOG_TAG, "Attempted to record session end without initialized recorder.");
            return;
        }

        final SessionInformation session = this.session;
        this.session = null;        // So it can't be double-recorded.

        if (session == null) {
            Log.w(LOG_TAG, "Unable to record session end: no session. Already ended?");
            return;
        }

        if (session.wallStartTime <= 0) {
            Log.e(LOG_TAG, "Session start " + session.wallStartTime + " isn't valid! Can't record end.");
            return;
        }

        long realEndTime = android.os.SystemClock.elapsedRealtime();
        try {
            JSONObject json = session.getCompletionJSON(reason, realEndTime);
            recordSessionEntry("normal", session, json);
        } catch (JSONException e) {
            Log.w(LOG_TAG, "Unable to generate session JSON.", e);

            // Continue so we don't hit it next time.
            // Future: record this exception in FHR's own error submitter.
        }

        // Track the end of this session in shared prefs, so it doesn't get
        // double-counted on next run.
        session.recordCompletion(editor);
    }
}

