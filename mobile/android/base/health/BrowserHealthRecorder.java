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
import org.mozilla.gecko.Distribution;
import org.mozilla.gecko.Distribution.DistributionDescriptor;
import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;

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
import java.util.Iterator;
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
 * #onAppLocaleChanged(String)} followed by {@link
 * #onEnvironmentChanged()}.
 *
 * Use it to record events: {@link #recordSearch(String, String)}.
 *
 * Shut it down when you're done being a browser: {@link #close()}.
 */
public class BrowserHealthRecorder implements GeckoEventListener {
    private static final String LOG_TAG = "GeckoHealthRec";
    private static final String PREF_ACCEPT_LANG = "intl.accept_languages";
    private static final String PREF_BLOCKLIST_ENABLED = "extensions.blocklist.enabled";
    private static final String EVENT_SNAPSHOT = "HealthReport:Snapshot";
    private static final String EVENT_ADDONS_CHANGE = "Addons:Change";
    private static final String EVENT_ADDONS_UNINSTALLING = "Addons:Uninstalling";
    private static final String EVENT_PREF_CHANGE = "Pref:Change";

    // This is raised from Gecko and signifies a search via the URL bar (not a bookmarks keyword
    // search). Using this event (rather than passing the invocation location as an arg) avoids
    // browser.js having to know about the invocation location.
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

        /**
         * Initialize a new SessionInformation instance to 'split' the current
         * session.
         */
        public static SessionInformation forRuntimeTransition() {
            final boolean wasOOM = false;
            final boolean wasStopped = true;
            final long wallStartTime = System.currentTimeMillis();
            final long realStartTime = android.os.SystemClock.elapsedRealtime();
            Log.v(LOG_TAG, "Recording runtime session transition: " +
                           wallStartTime + ", " + realStartTime);
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
     *
     * appLocale can be null, which indicates that it will be provided later.
     */
    public BrowserHealthRecorder(final Context context,
                                 final String profilePath,
                                 final EventDispatcher dispatcher,
                                 final String osLocale,
                                 final String appLocale,
                                 SessionInformation previousSession) {
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

        // Note that the PIC is not necessarily fully initialized at this point:
        // we haven't set the app locale. This must be done before an environment
        // is recorded.
        this.profileCache = new ProfileInformationCache(profilePath);
        try {
            this.initialize(context, profilePath, osLocale, appLocale);
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
        this.dispatcher.unregisterEventListener(EVENT_SNAPSHOT, this);
        this.dispatcher.unregisterEventListener(EVENT_ADDONS_CHANGE, this);
        this.dispatcher.unregisterEventListener(EVENT_ADDONS_UNINSTALLING, this);
        this.dispatcher.unregisterEventListener(EVENT_PREF_CHANGE, this);
        this.dispatcher.unregisterEventListener(EVENT_KEYWORD_SEARCH, this);
        this.dispatcher.unregisterEventListener(EVENT_SEARCH, this);
    }

    public void onAppLocaleChanged(String to) {
        Log.d(LOG_TAG, "Setting health recorder app locale to " + to);
        this.profileCache.beginInitialization();
        this.profileCache.setAppLocale(to);
    }

    public void onAddonChanged(String id, JSONObject json) {
        this.profileCache.beginInitialization();
        try {
            this.profileCache.updateJSONForAddon(id, json);
        } catch (IllegalStateException e) {
            Log.w(LOG_TAG, "Attempted to update add-on cache prior to full init.", e);
        }
    }

    public void onAddonUninstalling(String id) {
        this.profileCache.beginInitialization();
        try {
            this.profileCache.removeAddon(id);
        } catch (IllegalStateException e) {
            Log.w(LOG_TAG, "Attempted to update add-on cache prior to full init.", e);
        }
    }

    /**
     * Call this when a material change might have occurred in the running
     * environment, such that a new environment should be computed and prepared
     * for use in future events.
     *
     * Invoke this method after calls that mutate the environment.
     *
     * If this change resulted in a transition between two environments, {@link
     * #onEnvironmentTransition(int, int, boolean, String)} will be invoked on the background
     * thread.
     */
    public synchronized void onEnvironmentChanged() {
        onEnvironmentChanged(true, "E");
    }

    /**
     * If `startNewSession` is false, it means no new session should begin
     * (e.g., because we're about to restart, and we don't want to create
     * an orphan).
     */
    public synchronized void onEnvironmentChanged(final boolean startNewSession, final String sessionEndReason) {
        final int previousEnv = this.env;
        this.env = -1;
        try {
            profileCache.completeInitialization();
        } catch (java.io.IOException e) {
            Log.e(LOG_TAG, "Error completing profile cache initialization.", e);
            this.state = State.INITIALIZATION_FAILED;
            return;
        }

        final int updatedEnv = ensureEnvironment();

        if (updatedEnv == -1 ||
            updatedEnv == previousEnv) {
            Log.v(LOG_TAG, "Environment didn't change.");
            return;
        }
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                try {
                    onEnvironmentTransition(previousEnv, updatedEnv, startNewSession, sessionEndReason);
                } catch (Exception e) {
                    Log.w(LOG_TAG, "Could not record environment transition.", e);
                }
            }
        });
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

    private void onPrefMessage(final String pref, final JSONObject message) {
        Log.d(LOG_TAG, "Incorporating environment: " + pref);
        if (PREF_ACCEPT_LANG.equals(pref)) {
            // We only record whether this is user-set.
            try {
                this.profileCache.beginInitialization();
                this.profileCache.setAcceptLangUserSet(message.getBoolean("isUserSet"));
            } catch (JSONException ex) {
                Log.w(LOG_TAG, "Unexpected JSONException fetching isUserSet for " + pref);
            }
            return;
        }

        // (We only handle boolean prefs right now.)
        try {
            boolean value = message.getBoolean("value");

            if (AppConstants.TELEMETRY_PREF_NAME.equals(pref)) {
                this.profileCache.beginInitialization();
                this.profileCache.setTelemetryEnabled(value);
                return;
            }

            if (PREF_BLOCKLIST_ENABLED.equals(pref)) {
                this.profileCache.beginInitialization();
                this.profileCache.setBlocklistEnabled(value);
                return;
            }
        } catch (JSONException ex) {
            Log.w(LOG_TAG, "Unexpected JSONException fetching boolean value for " + pref);
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
                        dispatcher.registerEventListener(EVENT_ADDONS_UNINSTALLING, self);
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
                                         final String profilePath,
                                         final String osLocale,
                                         final String appLocale)
        throws java.io.IOException {

        Log.d(LOG_TAG, "Initializing profile cache.");
        this.state = State.INITIALIZING;

        // If we can restore state from last time, great.
        if (this.profileCache.restoreUnlessInitialized()) {
            this.profileCache.updateLocales(osLocale, appLocale);
            this.profileCache.completeInitialization();

            Log.d(LOG_TAG, "Successfully restored state. Initializing storage.");
            initializeStorage();
            return;
        }

        // Otherwise, let's initialize it from scratch.
        this.profileCache.beginInitialization();
        this.profileCache.setProfileCreationTime(getAndPersistProfileInitTime(context, profilePath));
        this.profileCache.setOSLocale(osLocale);
        this.profileCache.setAppLocale(appLocale);

        // Because the distribution lookup can take some time, do it at the end of
        // our background startup work, along with the Gecko snapshot fetch.
        final GeckoEventListener self = this;
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                final DistributionDescriptor desc = new Distribution(context).getDescriptor();
                if (desc != null && desc.valid) {
                    profileCache.setDistributionString(desc.id, desc.version);
                }
                Log.d(LOG_TAG, "Requesting all add-ons and FHR prefs from Gecko.");
                dispatcher.registerEventListener(EVENT_SNAPSHOT, self);
                GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("HealthReport:RequestSnapshot", null));
            }
        });
    }

    /**
     * Invoked in the background whenever the environment transitions between
     * two valid values.
     */
    protected void onEnvironmentTransition(int prev, int env, boolean startNewSession, String sessionEndReason) {
        if (this.state != State.INITIALIZED) {
            Log.d(LOG_TAG, "Not initialized: not recording env transition (" + prev + " => " + env + ").");
            return;
        }

        final SharedPreferences prefs = GeckoApp.getAppSharedPreferences();
        final SharedPreferences.Editor editor = prefs.edit();

        recordSessionEnd(sessionEndReason, editor, prev);

        if (!startNewSession) {
            editor.commit();
            return;
        }

        final SessionInformation newSession = SessionInformation.forRuntimeTransition();
        setCurrentSession(newSession);
        newSession.recordBegin(editor);
        editor.commit();
    }

    @Override
    public void handleMessage(String event, JSONObject message) {
        try {
            if (EVENT_SNAPSHOT.equals(event)) {
                Log.d(LOG_TAG, "Got all add-ons and prefs.");
                try {
                    JSONObject json = message.getJSONObject("json");
                    JSONObject addons = json.getJSONObject("addons");
                    Log.i(LOG_TAG, "Persisting " + addons.length() + " add-ons.");
                    profileCache.setJSONForAddons(addons);

                    JSONObject prefs = json.getJSONObject("prefs");
                    Log.i(LOG_TAG, "Persisting prefs.");
                    Iterator<?> keys = prefs.keys();
                    while (keys.hasNext()) {
                        String pref = (String) keys.next();
                        this.onPrefMessage(pref, prefs.getJSONObject(pref));
                    }

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

            if (EVENT_ADDONS_UNINSTALLING.equals(event)) {
                this.onAddonUninstalling(message.getString("id"));
                this.onEnvironmentChanged();
                return;
            }

            if (EVENT_ADDONS_CHANGE.equals(event)) {
                this.onAddonChanged(message.getString("id"), message.getJSONObject("json"));
                this.onEnvironmentChanged();
                return;
            }

            if (EVENT_PREF_CHANGE.equals(event)) {
                final String pref = message.getString("pref");
                Log.d(LOG_TAG, "Pref changed: " + pref);
                this.onPrefMessage(pref, message);
                this.onEnvironmentChanged();
                return;
            }

            // Searches.
            if (EVENT_KEYWORD_SEARCH.equals(event)) {
                // A search via the URL bar. Since we eliminate all other search possibilities
                // (e.g. bookmarks keyword, search suggestion) when we initially process the
                // search URL, this is considered a default search.
                recordSearch(message.getString("identifier"), "bartext");
                return;
            }
            if (EVENT_SEARCH.equals(event)) {
                if (!message.has("location")) {
                    Log.d(LOG_TAG, "Ignoring search without location.");
                    return;
                }
                recordSearch(message.optString("identifier", null), message.getString("location"));
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
    public static final int MEASUREMENT_VERSION_SEARCH_COUNTS = 5;

    public static final String[] SEARCH_LOCATIONS = {
        "barkeyword",
        "barsuggest",
        "bartext",
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
     * Record a search.
     *
     * @param engineID the string identifier for the engine. Can be <code>null</code>.
     * @param location one of a fixed set of locations: see {@link #SEARCH_LOCATIONS}.
     */
    public void recordSearch(final String engineID, final String location) {
        if (this.state != State.INITIALIZED) {
            Log.d(LOG_TAG, "Not initialized: not recording search. (" + this.state + ")");
            return;
        }

        if (location == null) {
            throw new IllegalArgumentException("location must be provided for search.");
        }

        final int day = storage.getDay();
        final int env = this.env;
        final String key = (engineID == null) ? "other" : engineID;
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
     * "sg": Gecko startup time. Present if this is a clean launch. This
     *       corresponds to the telemetry timer FENNEC_STARTUP_TIME_GECKOREADY.
     * "sj": Java activity init time. Present if this is a clean launch. This
     *       corresponds to the telemetry timer FENNEC_STARTUP_TIME_JAVAUI,
     *       and includes initialization tasks beyond initial
     *       onWindowFocusChanged.
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
    private void recordSessionEntry(String field, SessionInformation session, final int environment, JSONObject value) {
        final HealthReportDatabaseStorage storage = this.storage;
        if (storage == null) {
            Log.d(LOG_TAG, "No storage: not recording session entry. Shutting down?");
            return;
        }

        try {
            final int sessionField = storage.getField(MEASUREMENT_NAME_SESSIONS,
                                                      MEASUREMENT_VERSION_SESSIONS,
                                                      field)
                                            .getID();
            final int day = storage.getDay(session.wallStartTime);
            storage.recordDailyDiscrete(environment, day, sessionField, value);
            Log.v(LOG_TAG, "Recorded session entry for env " + environment + ", current is " + env);
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
            recordSessionEntry("abnormal", this.previousSession, this.env,
                               this.previousSession.getCrashedJSON());
        } catch (Exception e) {
            Log.w(LOG_TAG, "Unable to generate session JSON.", e);

            // Future: record this exception in FHR's own error submitter.
        }
    }

    public void recordSessionEnd(String reason, SharedPreferences.Editor editor) {
        recordSessionEnd(reason, editor, env);
    }

    /**
     * Record that the current session ended. Does not commit the provided editor.
     *
     * @param environment An environment ID. This allows callers to record the
     *                    end of a session due to an observed environment change.
     */
    public void recordSessionEnd(String reason, SharedPreferences.Editor editor, final int environment) {
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
            recordSessionEntry("normal", session, environment, json);
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

