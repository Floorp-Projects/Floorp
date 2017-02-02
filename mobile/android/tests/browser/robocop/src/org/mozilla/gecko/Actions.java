/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.util.GeckoBundle;

import android.database.Cursor;

public interface Actions {

    /** Special keys supported by sendSpecialKey() */
    public enum SpecialKey {
        DOWN,
        UP,
        LEFT,
        RIGHT,
        ENTER,
        MENU,
        DELETE,
    }

    public interface EventExpecter {
        /** Blocks until the event has been received. Subsequent calls will return immediately. */
        public void blockForEvent();
        public void blockForEvent(long millis, boolean failOnTimeout);

        /** Blocks until the event has been received and returns data associated with the event. */
        public String blockForEventData();

        /** Blocks until the event has been received and returns data associated with the event. */
        public GeckoBundle blockForBundle();

        /**
         * Blocks until the event has been received, or until the timeout has been exceeded.
         * Returns the data associated with the event, if applicable.
         */
        public String blockForEventDataWithTimeout(long millis);

        /**
         * Blocks until the event has been received, or until the timeout has been exceeded.
         * Returns the data associated with the event, if applicable.
         */
        public GeckoBundle blockForBundleWithTimeout(long millis);

        /** Polls to see if the event has been received. Once this returns true, subsequent calls will also return true. */
        public boolean eventReceived();

        /** Stop listening for events. */
        public void unregisterListener();
    }

    public interface RepeatedEventExpecter extends EventExpecter {
        /** Blocks until at least one event has been received, and no events have been received in the last <code>millis</code> milliseconds. */
        public void blockUntilClear(long millis);
    }

    public enum EventType {
        GECKO,
        UI,
        BACKGROUND
    }

    /**
     * Sends an event to the global EventDispatcher instance.
     *
     * @param event The event type
     * @param data Data associated with the event
     */
    void sendGlobalEvent(String event, GeckoBundle data);

    /**
     * Sends an event to the GeckoApp-specific EventDispatcher instance.
     *
     * @param event The event type
     * @param data Data associated with the event
     */
    void sendWindowEvent(String event, GeckoBundle data);

    public interface PrefWaiter {
        boolean isFinished();
        void waitForFinish();
        void waitForFinish(long timeoutMillis, boolean failOnTimeout);
    }

    public abstract static class PrefHandlerBase implements PrefsHelper.PrefHandler {
        /* package */ Assert asserter;

        @Override // PrefsHelper.PrefHandler
        public void prefValue(String pref, boolean value) {
            asserter.ok(false, "Unexpected pref callback", "");
        }

        @Override // PrefsHelper.PrefHandler
        public void prefValue(String pref, int value) {
            asserter.ok(false, "Unexpected pref callback", "");
        }

        @Override // PrefsHelper.PrefHandler
        public void prefValue(String pref, String value) {
            asserter.ok(false, "Unexpected pref callback", "");
        }

        @Override // PrefsHelper.PrefHandler
        public void finish() {
        }
    }

    PrefWaiter getPrefs(String[] prefNames, PrefHandlerBase handler);
    void setPref(String pref, Object value, boolean flush);
    PrefWaiter addPrefsObserver(String[] prefNames, PrefHandlerBase handler);
    void removePrefsObserver(PrefWaiter handler);

    /**
     * Listens for an event on the global EventDispatcher instance.
     * The returned object can be used to test if the event has been
     * received. Note that only one event is listened for.
     *
     * @param type The thread type for the event
     * @param event The name for the event
     */
    RepeatedEventExpecter expectGlobalEvent(EventType type, String event);

    /**
     * Listens for an event on the global EventDispatcher instance.
     * The returned object can be used to test if the event has been
     * received. Note that only one event is listened for.
     *
     * @param type The thread type for the event
     * @param event The name for the event
     */
    RepeatedEventExpecter expectWindowEvent(EventType type, String event);

    /**
     * Listens for a paint event. Note that calling expectPaint() will
     * invalidate the event expecters returned from any previous calls
     * to expectPaint(); calling any methods on those invalidated objects
     * will result in undefined behaviour.
     */
    RepeatedEventExpecter expectPaint();

    /**
     * Send a string to the application
     *
     * @param keysToSend The string to send
     */
    void sendKeys(String keysToSend);

    /**
     * Send a special keycode to the element
     *
     * @param key The special key to send
     */
    void sendSpecialKey(SpecialKey key);
    void sendKeyCode(int keyCode);

    void drag(int startingX, int endingX, int startingY, int endingY);

    /**
     * Run a sql query on the specified database
     */
    public Cursor querySql(String dbPath, String sql);
}
