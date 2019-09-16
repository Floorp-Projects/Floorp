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
import android.support.annotation.Nullable;
import android.support.annotation.NonNull;
import android.support.annotation.UiThread;
import android.util.Log;

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
}
