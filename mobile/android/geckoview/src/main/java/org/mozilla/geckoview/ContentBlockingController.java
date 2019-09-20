/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import org.json.JSONException;
import org.json.JSONObject;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.util.GeckoBundle;

import android.support.annotation.AnyThread;
import android.support.annotation.IntDef;
import android.support.annotation.Nullable;
import android.support.annotation.NonNull;
import android.support.annotation.UiThread;
import android.util.Log;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * ContentBlockingController is used to manage and modify the content
 * blocking exception list. This list is shared across all sessions.
 */
@AnyThread
public class ContentBlockingController {
    private static final String LOGTAG = "GeckoContentBlocking";

    /**
     * ExceptionList represents a content blocking exception list exported
     * from Gecko. It can be used to persist the list or to inspect the URIs
     * present in the list.
     */
    @AnyThread
    public class ExceptionList {
        private final @NonNull GeckoBundle mBundle;

        /* package */ ExceptionList(final @NonNull GeckoBundle bundle) {
            mBundle = new GeckoBundle(bundle);
        }

        /**
         * Returns the URIs currently on the content blocking exception list.
         *
         * @return A string array containing the URIs.
         */
        public @NonNull String[] getUris() {
            return mBundle.getStringArray("uris");
        }

        /**
         * Returns a string representation of the content blocking exception list.
         * May be null if the JSON to string conversion fails for any reason.
         *
         * @return A string representing the exception list.
         */
        @Override
        public @Nullable String toString() {
            String res;
            try {
                res = mBundle.toJSONObject().toString();
            } catch (JSONException e) {
                Log.e(LOGTAG, "Could not convert session state to string.");
                res = null;
            }

            return res;
        }

        /**
         * Returns a JSONObject representation of the content blocking exception list.
         *
         * @return A JSONObject representing the exception list.
         *
         * @throws JSONException if conversion to JSONObject fails.
         */
        public @NonNull JSONObject toJson() throws JSONException {
            return mBundle.toJSONObject();
        }

        /**
         * Creates a new exception list from a string. The string should be valid
         * output from {@link toString}.
         *
         * @param savedList A string representation of a saved exception list.
         *
         * @throws JSONException if the string representation no longer represents valid JSON.
         */
        public ExceptionList(final @NonNull String savedList) throws JSONException {
            mBundle = GeckoBundle.fromJSONObject(new JSONObject(savedList));
        }

        /**
         * Creates a new exception list from a JSONObject. The JSONObject should be valid
         * output from {@link toJson}.
         *
         * @param savedList A JSONObject representation of a saved exception list.
         *
         * @throws JSONException if the JSONObject cannot be converted for any reason.
         */
        public ExceptionList(final @NonNull JSONObject savedList) throws JSONException {
            mBundle = GeckoBundle.fromJSONObject(savedList);
        }

        /* package */ GeckoBundle getBundle() {
            return mBundle;
        }
    }

    /**
     * Add a content blocking exception for the site currently loaded by the supplied
     * {@link GeckoSession}.
     *
     * @param session A {@link GeckoSession} whose site will be added to the content
     *                blocking exceptions list.
     */
    @UiThread
    public void addException(final @NonNull GeckoSession session) {
        final GeckoBundle msg = new GeckoBundle(1);
        msg.putString("sessionId", session.getId());
        EventDispatcher.getInstance().dispatch("ContentBlocking:AddException", msg);
    }

    /**
     * Remove an exception for the site currently loaded by the supplied {@link GeckoSession}
     * from the content blocking exception list, if there is such an exception. If there is no
     * such exception, this is a no-op.
     *
     * @param session A {@link GeckoSession} whose site will be removed from the content
     *                blocking exceptions list.
     */
    @UiThread
    public void removeException(final @NonNull GeckoSession session) {
        final GeckoBundle msg = new GeckoBundle(1);
        msg.putString("sessionId", session.getId());
        EventDispatcher.getInstance().dispatch("ContentBlocking:RemoveException", msg);
    }

    /**
     * Check whether or not there is an exception for the site currently loaded by the
     * supplied {@link GeckoSession}.
     *
     * @param session A {@link GeckoSession} whose site will be checked against the content
     *                blocking exceptions list.
     *
     * @return A {@link GeckoResult} which resolves to a Boolean indicating whether or
     *         not the current site is on the exception list.
     */
    @UiThread
    public @NonNull GeckoResult<Boolean> checkException(final @NonNull GeckoSession session) {
        final CallbackResult<Boolean> result = new CallbackResult<Boolean>() {
            @Override
            public void sendSuccess(final Object value) {
                complete((Boolean) value);
            }
        };
        final GeckoBundle msg = new GeckoBundle(1);
        msg.putString("sessionId", session.getId());
        EventDispatcher.getInstance().dispatch("ContentBlocking:CheckException", msg, result);
        return result;
    }

    /**
     * Save the current content blocking exception list as an {@link ExceptionList}.
     *
     * @return An {@link ExceptionList} which can be used to restore the current
     *         exception list.
     */
    @UiThread
    public @NonNull GeckoResult<ExceptionList> saveExceptionList() {
        final CallbackResult<ExceptionList> result = new CallbackResult<ExceptionList>() {
            @Override
            public void sendSuccess(final Object value) {
                complete(new ExceptionList((GeckoBundle) value));
            }
        };
        EventDispatcher.getInstance().dispatch("ContentBlocking:SaveList", null, result);
        return result;
    }

    /**
     * Restore the supplied {@link ExceptionList}, overwriting the existing exception list.
     *
     * @param list An {@link ExceptionList} originally created by {@link saveExceptionList}.
     */
    @UiThread
    public void restoreExceptionList(final @NonNull ExceptionList list) {
        EventDispatcher.getInstance().dispatch("ContentBlocking:RestoreList", list.getBundle());
    }

    /**
     * Clear the content blocking exception list entirely.
     */
    @UiThread
    public void clearExceptionList() {
        EventDispatcher.getInstance().dispatch("ContentBlocking:ClearList", null);
    }

    public static class Event {
        // These values must be kept in sync with the corresponding values in
        // nsIWebProgressListener.idl.
        /**
         * Tracking content has been blocked from loading.
         */
        public static final int BLOCKED_TRACKING_CONTENT        = 0x00001000;

        /**
         * Tracking content has been loaded.
         */
        public static final int LOADED_TRACKING_CONTENT         = 0x00002000;

        /**
         * Fingerprinting content has been blocked from loading.
         */
        public static final int BLOCKED_FINGERPRINTING_CONTENT  = 0x00000040;

        /**
         * Fingerprinting content has been loaded.
         */
        public static final int LOADED_FINGERPRINTING_CONTENT   = 0x00000400;

        /**
         * Cryptomining content has been blocked from loading.
         */
        public static final int BLOCKED_CRYPTOMINING_CONTENT    = 0x00000800;

        /**
         * Cryptomining content has been loaded.
         */
        public static final int LOADED_CRYPTOMINING_CONTENT     = 0x00200000;

        /**
         * Content which appears on the SafeBrowsing list has been blocked from loading.
         */
        public static final int BLOCKED_UNSAFE_CONTENT          = 0x00004000;

        /**
         * Performed a storage access check, which usually means something like a
         * cookie or a storage item was loaded/stored on the current tab.
         * Alternatively this could indicate that something in the current tab
         * attempted to communicate with its same-origin counterparts in other
         * tabs.
         */
        public static final int COOKIES_LOADED                  = 0x00008000;

        /**
         * Similar to {@link COOKIES_LOADED}, but only sent if the subject of the
         * action was a third-party tracker when the active cookie policy imposes
         * restrictions on such content.
         */
        public static final int COOKIES_LOADED_TRACKER          = 0x00040000;

        /**
         * Similar to {@link COOKIES_LOADED}, but only sent if the subject of the
         * action was a third-party social tracker when the active cookie policy
         * imposes restrictions on such content.
         */
        public static final int COOKIES_LOADED_SOCIALTRACKER    = 0x00080000;

        /**
         * Rejected for custom site permission.
         */
        public static final int COOKIES_BLOCKED_BY_PERMISSION   = 0x10000000;

        /**
         * Rejected because the resource is a tracker and cookie policy doesn't
         * allow its loading.
         */
        public static final int COOKIES_BLOCKED_TRACKER         = 0x20000000;

        /**
         * Rejected because cookie policy blocks all cookies.
         */
        public static final int COOKIES_BLOCKED_ALL             = 0x40000000;

        /**
         * Rejected because the resource is a third-party and cookie policy forces
         * third-party resources to be partitioned.
         */
        public static final int COOKIES_PARTITIONED_FOREIGN     = 0x80000000;

        /**
         * Rejected because cookie policy blocks 3rd party cookies.
         */
        public static final int COOKIES_BLOCKED_FOREIGN         = 0x00000080;

        /**
         * SocialTracking content has been blocked from loading.
         */
        public static final int BLOCKED_SOCIALTRACKING_CONTENT  = 0x00010000;

        /**
         * SocialTracking content has been loaded.
         */
        public static final int LOADED_SOCIALTRACKING_CONTENT   = 0x00020000;

        protected Event() {}
    }

    /**
     * An entry in the content blocking log for a site.
     */
    @AnyThread
    public static class LogEntry {
        /**
         * Data about why a given entry was blocked.
         */
        public static class BlockingData {
            @Retention(RetentionPolicy.SOURCE)
            @IntDef({ Event.BLOCKED_TRACKING_CONTENT, Event.LOADED_TRACKING_CONTENT,
                      Event.BLOCKED_FINGERPRINTING_CONTENT, Event.LOADED_FINGERPRINTING_CONTENT,
                      Event.BLOCKED_CRYPTOMINING_CONTENT, Event.LOADED_CRYPTOMINING_CONTENT,
                      Event.BLOCKED_UNSAFE_CONTENT, Event.COOKIES_LOADED,
                      Event.COOKIES_LOADED_TRACKER, Event.COOKIES_LOADED_SOCIALTRACKER,
                      Event.COOKIES_BLOCKED_BY_PERMISSION, Event.COOKIES_BLOCKED_TRACKER,
                      Event.COOKIES_BLOCKED_ALL, Event.COOKIES_PARTITIONED_FOREIGN,
                      Event.COOKIES_BLOCKED_FOREIGN, Event.BLOCKED_SOCIALTRACKING_CONTENT,
                      Event.LOADED_SOCIALTRACKING_CONTENT })
            /* package */ @interface LogEvent {}

            /**
             * A category the entry falls under.
             */
            public final @LogEvent int category;

            /**
             * Indicates whether or not blocking occured for this category,
             * where applicable.
             */
            public final boolean blocked;

            /**
             * The count of consecutive repeated appearances.
             */
            public final int count;

            /* package */ BlockingData(final @NonNull GeckoBundle bundle) {
                category = bundle.getInt("category");
                blocked = bundle.getBoolean("blocked");
                count = bundle.getInt("count");
            }

            protected BlockingData() {
                category = 0;
                blocked = false;
                count = 0;
            }
        }

        /**
         * The origin of this log entry.
         */
        public final @NonNull String origin;

        /**
         * The blocking data for this origin, sorted chronologically.
         */
        public final @NonNull List<BlockingData> blockingData;

        /* package */ LogEntry(final @NonNull GeckoBundle bundle) {
            origin = bundle.getString("origin");
            final GeckoBundle[] data = bundle.getBundleArray("blockData");
            final ArrayList<BlockingData> dataArray = new ArrayList<BlockingData>(data.length);
            for (GeckoBundle b : data) {
                dataArray.add(new BlockingData(b));
            }
            blockingData = Collections.unmodifiableList(dataArray);
        }

        protected LogEntry() {
            origin = null;
            blockingData = null;
        }
    }

    /**
     * Get a log of all content blocking information for the site currently loaded by the
     * supplied {@link GeckoSession}.
     *
     * @param session A {@link GeckoSession} for which you want the content blocking log.
     *
     * @return A {@link GeckoResult} that resolves to the list of content blocking log entries.
     */
    @UiThread
    public @NonNull GeckoResult<List<LogEntry>> getLog(final @NonNull GeckoSession session) {
        final CallbackResult<List<LogEntry>> result = new CallbackResult<List<LogEntry>>() {
            @Override
            public void sendSuccess(final Object value) {
                final GeckoBundle[] bundles = ((GeckoBundle) value).getBundleArray("log");
                final ArrayList<LogEntry> logArray = new ArrayList<LogEntry>(bundles.length);
                for (GeckoBundle b : bundles) {
                    logArray.add(new LogEntry(b));
                }
                complete(Collections.unmodifiableList(logArray));
            }
        };
        session.getEventDispatcher().dispatch("ContentBlocking:RequestLog", null, result);
        return result;
    }
}
