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

import androidx.annotation.AnyThread;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.UiThread;

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

    @AnyThread
    public static class ContentBlockingException {
        private final @NonNull String mEncodedPrincipal;

        /**
         * A String representing the URI of this content blocking exception.
         */
        public final @NonNull String uri;

        /* package */ ContentBlockingException(final @NonNull String encodedPrincipal,
                                               final @NonNull String uri) {
            mEncodedPrincipal = encodedPrincipal;
            this.uri = uri;
        }

        /**
         * Returns a JSONObject representation of the content blocking exception.
         *
         * @return A JSONObject representing the exception.
         *
         * @throws JSONException if conversion to JSONObject fails.
         */
        public @NonNull JSONObject toJson() throws JSONException {
            final JSONObject res = new JSONObject();
            res.put("principal", mEncodedPrincipal);
            res.put("uri", uri);
            return res;
        }

        /**
         *
         * Returns a ContentBlockingException reconstructed from JSON.
         *
         * @param savedException A JSONObject representation of a saved exception; should be the output of
         *                       {@link #toJson}.
         *
         * @return A ContentBlockingException reconstructed from the supplied JSONObject.
         *
         * @throws JSONException if the JSONObject cannot be converted for any reason.
         */
        public static @NonNull ContentBlockingException fromJson(final @NonNull JSONObject savedException) throws JSONException {
            return new ContentBlockingException(savedException.getString("principal"), savedException.getString("uri"));
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
     * Remove the exception specified by the supplied {@link ContentBlockingException} from
     * the content blocking exception list, if it is present. If there is no such exception,
     * this is a no-op.
     *
     * @param exception A {@link ContentBlockingException} which will be removed from the
     *                  content blocking exception list.
     */
    @AnyThread
    public void removeException(final @NonNull ContentBlockingException exception) {
        final GeckoBundle msg = new GeckoBundle(1);
        msg.putString("principal", exception.mEncodedPrincipal);
        EventDispatcher.getInstance().dispatch("ContentBlocking:RemoveExceptionByPrincipal", msg);
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
        final GeckoBundle msg = new GeckoBundle(1);
        msg.putString("sessionId", session.getId());
        return EventDispatcher.getInstance()
                .queryBoolean("ContentBlocking:CheckException", msg);
    }

    private List<ContentBlockingException> exceptionListFromBundle(final GeckoBundle value) {
        final String[] principals = value.getStringArray("principals");
        final String[] uris = value.getStringArray("uris");

        if (principals == null || uris == null) {
            throw new RuntimeException("Received invalid content blocking exception list");
        }

        final ArrayList<ContentBlockingException> res = new ArrayList<>(principals.length);

        for (int i = 0; i < principals.length; i++) {
            res.add(new ContentBlockingException(principals[i], uris[i]));
        }

        return Collections.unmodifiableList(res);
    }

    /**
     * Save the current content blocking exception list as a List of {@link ContentBlockingException}.
     *
     * @return A List of {@link ContentBlockingException} which can be used to restore or
     *         inspect the current exception list.
     */
    @UiThread
    public @NonNull GeckoResult<List<ContentBlockingException>> saveExceptionList() {
        return EventDispatcher.getInstance()
                .queryBundle("ContentBlocking:SaveList")
                .map(this::exceptionListFromBundle);
    }

    /**
     * Restore the supplied List of {@link ContentBlockingException}, overwriting the existing exception list.
     *
     * @param list A List of {@link ContentBlockingException} originally created by {@link #saveExceptionList}.
     */
    @AnyThread
    public void restoreExceptionList(final @NonNull List<ContentBlockingException> list) {
        final GeckoBundle bundle = new GeckoBundle(2);
        final String[] principals = new String[list.size()];
        final String[] uris = new String[list.size()];

        for (int i = 0; i < list.size(); i++) {
            principals[i] = list.get(i).mEncodedPrincipal;
            uris[i] = list.get(i).uri;
        }

        bundle.putStringArray("principals", principals);
        bundle.putStringArray("uris", uris);

        EventDispatcher.getInstance().dispatch("ContentBlocking:RestoreList", bundle);
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
         * Level 1 tracking content has been loaded.
         */
        public static final int LOADED_LEVEL_1_TRACKING_CONTENT = 0x00002000;

        /**
         * Level 2 tracking content has been loaded.
         */
        public static final int LOADED_LEVEL_2_TRACKING_CONTENT = 0x00100000;

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
         * Similar to {@link #COOKIES_LOADED}, but only sent if the subject of the
         * action was a third-party tracker when the active cookie policy imposes
         * restrictions on such content.
         */
        public static final int COOKIES_LOADED_TRACKER          = 0x00040000;

        /**
         * Similar to {@link #COOKIES_LOADED}, but only sent if the subject of the
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
         * Rejected because the resource is a tracker from a social origin and
         * cookie policy doesn't allow its loading.
         */
        public static final int COOKIES_BLOCKED_SOCIALTRACKER   = 0x01000000;

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

        /**
         * Indicates that content that would have been blocked has instead been
         * replaced with a shim.
         */
        public static final int REPLACED_TRACKING_CONTENT       = 0x00000010;

        /**
         * Indicates that content that would have been blocked has instead been
         * allowed by a shim.
         */
        public static final int ALLOWED_TRACKING_CONTENT        = 0x00000020;

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
            @IntDef({ Event.BLOCKED_TRACKING_CONTENT, Event.LOADED_LEVEL_1_TRACKING_CONTENT,
                      Event.LOADED_LEVEL_2_TRACKING_CONTENT, Event.BLOCKED_FINGERPRINTING_CONTENT,
                      Event.LOADED_FINGERPRINTING_CONTENT, Event.BLOCKED_CRYPTOMINING_CONTENT,
                      Event.LOADED_CRYPTOMINING_CONTENT, Event.BLOCKED_UNSAFE_CONTENT,
                      Event.COOKIES_LOADED, Event.COOKIES_LOADED_TRACKER,
                      Event.COOKIES_LOADED_SOCIALTRACKER, Event.COOKIES_BLOCKED_BY_PERMISSION,
                      Event.COOKIES_BLOCKED_TRACKER, Event.COOKIES_BLOCKED_SOCIALTRACKER,
                      Event.COOKIES_BLOCKED_ALL, Event.COOKIES_PARTITIONED_FOREIGN,
                      Event.COOKIES_BLOCKED_FOREIGN, Event.BLOCKED_SOCIALTRACKING_CONTENT,
                      Event.LOADED_SOCIALTRACKING_CONTENT, Event.REPLACED_TRACKING_CONTENT })
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
            for (final GeckoBundle b : data) {
                dataArray.add(new BlockingData(b));
            }
            blockingData = Collections.unmodifiableList(dataArray);
        }

        protected LogEntry() {
            origin = null;
            blockingData = null;
        }
    }

    private List<LogEntry> logFromBundle(final GeckoBundle value) {
        final GeckoBundle[] bundles = value.getBundleArray("log");
        final ArrayList<LogEntry> logArray = new ArrayList<>(bundles.length);
        for (final GeckoBundle b : bundles) {
            logArray.add(new LogEntry(b));
        }
        return Collections.unmodifiableList(logArray);
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
        return session.getEventDispatcher()
                .queryBundle("ContentBlocking:RequestLog")
                .map(this::logFromBundle);
    }
}
