/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

import android.support.annotation.AnyThread;
import android.support.annotation.IntDef;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.UiThread;
import android.util.Log;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;

/**
 * The Login Storage API provides a storage-level delegate to leverage Gecko's
 * complete range of heuristics for login forms, autofill and autocomplete
 * scenarios.
 *
 * <h2>Examples</h2>
 *
 * <h3>Autofill/Fetch API</h3>
 * <p>
 * GeckoView loads <code>https://example.com</code> which contains (for the
 * purpose of this example) elements resembling a login form, e.g.,
 * <pre><code>
 *   &lt;form&gt;
 *     &lt;input type=&quot;text&quot; placeholder=&quot;username&quot;&gt;
 *     &lt;input type=&quot;password&quot; placeholder=&quot;password&quot;&gt;
 *     &lt;input type=&quot;submit&quot; value=&quot;submit&quot;&gt;
 *   &lt;/form&gt;
 * </code></pre>
 * <p>
 * With the document parsed and the login input fields identified, GeckoView
 * dispatches a
 * <code>LoginStorage.Delegate.onLoginFetch(&quot;example.com&quot;)</code>
 * request to fetch logins for the given domain.
 * </p>
 *
 * <p>
 * Based on the provided login entries, GeckoView will attempt to autofill the
 * login input fields.
 * </p>
 *
 * <h3>Update API</h3>
 * <p>
 * When the user submits some login input fields, GeckoView dispatches another
 * <code>LoginStorage.Delegate.onLoginFetch(&quot;example.com&quot;)</code>
 * request to check whether the submitted login exists or whether it's a new or
 * updated login entry.
 * </p>
 * <p>
 * If the submitted login is already contained as-is in the collection returned
 * by <code>onLoginFetch</code>, then GeckoView dispatches
 * <code>LoginStorage.Delegate.onLoginUsed</code> with the submitted login
 * entry.
 * </p>
 * <p>
 * If the submitted login is a new or updated entry, GeckoView dispatches
 * a sequence of requests to save/update the login entry, see the Save API
 * example.
 * </p>
 *
 * <h3>Save API</h3>
 *
 * <p>
 * The user enters new or updated (password) login credentials in some login
 * input fields and submits explicitely (submit action) or by navigation.
 * GeckoView identifies the entered credentials and dispatches a
 * <code>GeckoSession.PromptDelegate.onLoginStoragePrompt(session, prompt)</code>
 * request with the <code>prompt</code> being of type
 * <code>LoginStoragePrompt.Type.SAVE</code> and containing the entered
 * credentials.
 * </p>
 *
 * <p>
 * The app may dismiss the prompt request via
 * <code>return GeckoResult.fromValue(prompt.dismiss())</code>
 * which terminates this saving request, or confirm it via
 * <code>return GeckoResult.fromValue(prompt.confirm(login))</code>
 * where <code>login</code> either holds the credentials originally provided by
 * the prompt request (<code>prompt.logins[0]</code>) or a new or modified login
 * entry.
 * </p>
 *
 * <p>
 * The login entry returned in a confirmed save prompt is used to request for
 * saving in the runtime delegate via
 * <code>LoginStorage.Delegate.onLoginSave(login)</code>.
 * If the app has already stored the entry during the prompt request handling,
 * it may ignore this storage saving request.
 * </p>
 *
 * <br>@see GeckoRuntime#setLoginStorageDelegate
 * <br>@see GeckoSession#setPromptDelegate
 * <br>@see GeckoSession.PromptDelegate#onLoginStoragePrompt
 */
public class LoginStorage {
    private static final String LOGTAG = "LoginStorage";
    private static final boolean DEBUG = false;

    /**
     * Holds login information for a specific entry.
     */
    public static class LoginEntry {
        private static final String GUID_KEY = "guid";
        private static final String ORIGIN_KEY = "origin";
        private static final String FORM_ACTION_ORIGIN_KEY = "formActionOrigin";
        private static final String HTTP_REALM_KEY = "httpRealm";
        private static final String USERNAME_KEY = "username";
        private static final String PASSWORD_KEY = "password";

        /**
         * The unique identifier for this login entry.
         */
        public final @Nullable String guid;

        /**
         * The origin this login entry applies to.
         */
        public final @NonNull String origin;

        /**
         * The origin this login entry was submitted to.
         * This only applies to form-based login entries.
         * It's derived from the action attribute set on the form element.
         */
        public final @Nullable String formActionOrigin;

        /**
         * The HTTP realm this login entry was requested for.
         * This only applies to non-form-based login entries.
         * It's derived from the WWW-Authenticate header set in a HTTP 401
         * response, see RFC2617 for details.
         */
        public final @Nullable String httpRealm;

        /**
         * The username for this login entry.
         */
        public final @NonNull String username;

        /**
         * The password for this login entry.
         */
        public final @NonNull String password;

        // For tests only.
        @AnyThread
        protected LoginEntry() {
            guid = null;
            origin = "";
            formActionOrigin = null;
            httpRealm = null;
            username = "";
            password = "";
        }

        @AnyThread
        /* package */ LoginEntry(final @NonNull GeckoBundle bundle) {
            guid = bundle.getString(GUID_KEY);
            origin = bundle.getString(ORIGIN_KEY);
            formActionOrigin = bundle.getString(FORM_ACTION_ORIGIN_KEY);
            httpRealm = bundle.getString(HTTP_REALM_KEY);
            username = bundle.getString(USERNAME_KEY);
            password = bundle.getString(PASSWORD_KEY);
        }

        @Override
        @AnyThread
        public String toString() {
            StringBuilder builder = new StringBuilder("LoginEntry {");
            builder
                .append("guid=").append(guid)
                .append(", origin=").append(origin)
                .append(", formActionOrigin=").append(formActionOrigin)
                .append(", httpRealm=").append(httpRealm)
                .append(", username=").append(username)
                .append(", password=").append(password)
                .append("}");
            return builder.toString();
        }

        @AnyThread
        /* package */ @NonNull GeckoBundle toBundle() {
            final GeckoBundle bundle = new GeckoBundle(6);
            bundle.putString(GUID_KEY, guid);
            bundle.putString(ORIGIN_KEY, origin);
            bundle.putString(FORM_ACTION_ORIGIN_KEY, formActionOrigin);
            bundle.putString(HTTP_REALM_KEY, httpRealm);
            bundle.putString(USERNAME_KEY, username);
            bundle.putString(PASSWORD_KEY, password);

            return bundle;
        }

        public static class Builder {
            private final GeckoBundle mBundle;

            @AnyThread
            /* package */ Builder(final @NonNull GeckoBundle bundle) {
                mBundle = new GeckoBundle(bundle);
            }

            @AnyThread
            public Builder() {
                mBundle = new GeckoBundle(6);
            }

            /**
             * Finalize the {@link LoginEntry} instance.
             *
             * @return The {@link LoginEntry} instance.
             */
            @AnyThread
            public @NonNull LoginEntry build() {
                return new LoginEntry(mBundle);
            }

            /**
             * Set the unique identifier for this login entry.
             *
             * @param guid The unique identifier string.
             * @return This {@link Builder} instance.
             */
            @AnyThread
            public @NonNull Builder guid(final @Nullable String guid) {
                mBundle.putString(GUID_KEY, guid);
                return this;
            }

            /**
             * Set the origin this login entry applies to.
             *
             * @param origin The origin string.
             * @return This {@link Builder} instance.
             */
            @AnyThread
            public @NonNull Builder origin(final @NonNull String origin) {
                mBundle.putString(ORIGIN_KEY, origin);
                return this;
            }

            /**
             * Set the origin this login entry was submitted to.
             *
             * @param formActionOrigin The form action origin string.
             * @return This {@link Builder} instance.
             */
            @AnyThread
            public @NonNull Builder formActionOrigin(
                    final @Nullable String formActionOrigin) {
                mBundle.putString(FORM_ACTION_ORIGIN_KEY, formActionOrigin);
                return this;
            }

            /**
             * Set the HTTP realm this login entry was requested for.
             *
             * @param httpRealm The HTTP realm string.
             * @return This {@link Builder} instance.
             */
            @AnyThread
            public @NonNull Builder httpRealm(final @Nullable String httpRealm) {
                mBundle.putString(HTTP_REALM_KEY, httpRealm);
                return this;
            }

            /**
             * Set the username for this login entry.
             *
             * @param username The username string.
             * @return This {@link Builder} instance.
             */
            @AnyThread
            public @NonNull Builder username(final @NonNull String username) {
                mBundle.putString(USERNAME_KEY, username);
                return this;
            }

            /**
             * Set the password for this login entry.
             *
             * @param password The password string.
             * @return This {@link Builder} instance.
             */
            @AnyThread
            public @NonNull Builder password(final @NonNull String password) {
                mBundle.putString(PASSWORD_KEY, password);
                return this;
            }
        }
    }

    @Retention(RetentionPolicy.SOURCE)
    @IntDef(flag = true,
            value = { UsedField.PASSWORD })
    /* package */ @interface LSUsedField {}

    // Sync with UsedField in GeckoViewLoginStorage.jsm.
    /**
     * Possible login entry field types for {@link Delegate#onLoginUsed}.
     */
    public static class UsedField {
        /**
         * The password field of a login entry.
         */
        public static final int PASSWORD = 1;
    }

    /**
     * Implement this interface to handle runtime login storage requests.
     * Login storage events include login entry requests for autofill and
     * autocompletion of login input fields.
     * This delegate is attached to the runtime via
     * {@link GeckoRuntime#setLoginStorageDelegate}.
     */
    public interface Delegate {
        /**
         * Request login entries for a given domain.
         * While processing the web document, we have identified elements
         * resembling login input fields suitable for autofill.
         * We will attempt to match the provided login information to the
         * identified input fields.
         *
         * @param domain The domain string for the requested logins.
         * @return A {@link GeckoResult} that completes with an array of
         *         {@link LoginEntry} containing the existing logins for the
         *         given domain.
         */
        @UiThread
        default @Nullable GeckoResult<LoginEntry[]> onLoginFetch(
                @NonNull String domain) {
            return null;
        }

        /**
         * Request saving or updating of the given login entry.
         * This is triggered by confirming a
         * {@link GeckoSession.PromptDelegate#onLoginStoragePrompt onLoginStoragePrompt}
         * request of type
         * {@link GeckoSession.PromptDelegate.LoginStoragePrompt.Type#SAVE Type.SAVE}.
         *
         * @param login The {@link LoginEntry} as confirmed by the prompt
         *              request.
         */
        @UiThread
        default void onLoginSave(@NonNull LoginEntry login) {}

        /**
         * Notify that the given login was used to autofill login input fields.
         * This is triggered by autofilling elements with unmodified login
         * entries as provided via {@link #onLoginFetch}.
         *
         * @param login The {@link LoginEntry} that was used for the
         *              autofilling.
         * @param usedFields The login entry fields used for autofilling.
         *                   A combination of {@link UsedField}.
         */
        @UiThread
        default void onLoginUsed(
                @NonNull LoginEntry login,
                @LSUsedField int usedFields) {}
    }

    /* package */ final static class Proxy implements BundleEventListener {
        private static final String LOGTAG = "LoginStorageProxy";

        private static final String FETCH_EVENT = "GeckoView:LoginStorage:Fetch";
        private static final String SAVE_EVENT = "GeckoView:LoginStorage:Save";
        private static final String USED_EVENT = "GeckoView:LoginStorage:Used";

        private @Nullable Delegate mDelegate;

        public Proxy() {}

        private void registerListener() {
            EventDispatcher.getInstance().registerUiThreadListener(
                    this,
                    FETCH_EVENT,
                    SAVE_EVENT,
                    USED_EVENT);
        }

        private void unregisterListener() {
            EventDispatcher.getInstance().unregisterUiThreadListener(
                    this,
                    FETCH_EVENT,
                    SAVE_EVENT,
                    USED_EVENT);
        }

        public synchronized void setDelegate(final @Nullable Delegate delegate) {
            if (mDelegate == null && delegate != null) {
                registerListener();
            } else if (mDelegate != null && delegate == null) {
                unregisterListener();
            }

            mDelegate = delegate;
        }

        public synchronized @Nullable Delegate getDelegate() {
            return mDelegate;
        }

        @Override // BundleEventListener
        public synchronized void handleMessage(
                final String event,
                final GeckoBundle message,
                final EventCallback callback) {
            if (DEBUG) {
                Log.d(LOGTAG, "handleMessage " + event);
            }

            if (mDelegate == null) {
                if (callback != null) {
                    callback.sendError("No LoginStorage delegate attached");
                }
                return;
            }

            if (FETCH_EVENT.equals(event)) {
                final String domain = message.getString("domain");
                final GeckoResult<LoginStorage.LoginEntry[]> result =
                    mDelegate.onLoginFetch(domain);

                if (result == null) {
                    callback.sendSuccess(new GeckoBundle[0]);
                    return;
                }

                result.accept(
                    logins -> {
                        if (logins == null) {
                            callback.sendSuccess(new GeckoBundle[0]);
                        }

                        // This is a one-liner with streams (API level 24).
                        final GeckoBundle[] loginBundles =
                            new GeckoBundle[logins.length];
                        for (int i = 0; i < logins.length; ++i) {
                            loginBundles[i] = logins[i].toBundle();
                        }

                        callback.sendSuccess(loginBundles);
                    },
                    exception -> callback.sendError(exception.getMessage()));
            } else if (SAVE_EVENT.equals(event)) {
                final GeckoBundle loginBundle = message.getBundle("login");
                final LoginEntry login = new LoginEntry(loginBundle);

                mDelegate.onLoginSave(login);
            } else if (USED_EVENT.equals(event)) {
                final GeckoBundle loginBundle = message.getBundle("login");
                final LoginEntry login = new LoginEntry(loginBundle);
                final int fields = message.getInt("usedFields");

                mDelegate.onLoginUsed(login, fields);
            }
        }
    }
}
