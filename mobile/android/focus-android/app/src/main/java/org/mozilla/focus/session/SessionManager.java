/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.session;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.VisibleForTesting;
import android.text.TextUtils;

import org.mozilla.focus.architecture.NonNullLiveData;
import org.mozilla.focus.architecture.NonNullMutableLiveData;
import org.mozilla.focus.customtabs.CustomTabConfig;
import org.mozilla.focus.shortcut.HomeScreen;
import org.mozilla.focus.utils.SafeIntent;
import org.mozilla.focus.utils.UrlUtils;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Sessions are managed by this global SessionManager instance.
 */
public class SessionManager {
    private static final SessionManager INSTANCE = new SessionManager();

    private NonNullMutableLiveData<List<Session>> sessions;
    private String currentSessionUUID;

    public static SessionManager getInstance() {
        return INSTANCE;
    }

    private SessionManager() {
        this.sessions = new NonNullMutableLiveData<>(
                Collections.unmodifiableList(Collections.<Session>emptyList()));
    }

    /**
     * Handle this incoming intent (via onCreate()) and create a new session if required.
     */
    public void handleIntent(final Context context, final SafeIntent intent, final Bundle savedInstanceState) {
        if ((intent.getFlags() & Intent.FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY) != 0) {
            // This Intent was launched from history (recent apps). Android will redeliver the
            // original Intent (which might be a VIEW intent). However if there's no active browsing
            // session then we do not want to re-process the Intent and potentially re-open a website
            // from a session that the user already "erased".
            return;
        }

        if (savedInstanceState != null) {
            // We are restoring a previous session - No need to handle this Intent.
            return;
        }

        createSessionFromIntent(context, intent);
    }

    /**
     * Handle this incoming intent (via onNewIntent()) and create a new session if required.
     */
    public void handleNewIntent(final Context context, final SafeIntent intent) {
        createSessionFromIntent(context, intent);
    }

    private void createSessionFromIntent(Context context, SafeIntent intent) {
        final String action = intent.getAction();

        if (Intent.ACTION_VIEW.equals(action)) {
            final Source source;
            if (intent.hasExtra(HomeScreen.ADD_TO_HOMESCREEN_TAG)) {
                source = Source.HOME_SCREEN;
                boolean blockingEnabled = intent.getBooleanExtra(HomeScreen.BLOCKING_ENABLED, true);
                createSession(context, source, intent, intent.getDataString(), blockingEnabled);
            } else {
                source = Source.VIEW;
                createSession(context, source, intent, intent.getDataString());
            }
        } else if (Intent.ACTION_SEND.equals(action)) {
            final String dataString = intent.getStringExtra(Intent.EXTRA_TEXT);
            if (!TextUtils.isEmpty(dataString)) {
                final boolean isSearch = !UrlUtils.isUrl(dataString);

                final String url = isSearch
                        ? UrlUtils.createSearchUrl(context, dataString)
                        : dataString;

                if (isSearch) {
                    createSearchSession(Source.SHARE, url, dataString);
                } else {
                    createSession(Source.SHARE, url);
                }
            }
        }
    }

    /**
     * Is there at least one browsing session?
     */
    public boolean hasSession() {
        return !sessions.getValue().isEmpty();
    }

    /**
     * Get the current session. This method will throw an exception if there's no active session.
     */
    public Session getCurrentSession() {
        if (currentSessionUUID == null) {
            throw new IllegalAccessError("There's no active session");
        }

        return getSessionByUUID(currentSessionUUID);
    }

    public boolean isCurrentSession(@NonNull Session session) {
        return session.getUUID().equals(currentSessionUUID);
    }

    public boolean hasSessionWithUUID(@NonNull String uuid) {
        for (Session session : sessions.getValue()) {
            if (uuid.equals(session.getUUID())) {
                return true;
            }
        }

        return false;
    }

    public Session getSessionByUUID(@NonNull String uuid) {
        for (Session session : sessions.getValue()) {
            if (uuid.equals(session.getUUID())) {
                return session;
            }
        }

        throw new IllegalAccessError("There's no active session with UUID " + uuid);
    }

    public int getNumberOfSessions() {
        return sessions.getValue().size();
    }

    public int getPositionOfCurrentSession() {
        if (currentSessionUUID == null) {
            return -1;
        }

        for (int i = 0; i < this.sessions.getValue().size(); i++) {
            final Session session = this.sessions.getValue().get(i);

            if (session.getUUID().equals(currentSessionUUID)) {
                return i;
            }
        }

        return -1;
    }

    public NonNullLiveData<List<Session>> getSessions() {
        return sessions;
    }

    public void createSession(@NonNull Source source, @NonNull String url) {
        final Session session = new Session(source, url);
        addSession(session);
    }

    public void createSearchSession(@NonNull Source source, @NonNull String url, String searchTerms) {
        final Session session = new Session(source, url);
        session.setSearchTerms(searchTerms);
        addSession(session);
    }

    private void createSession(Context context, Source source, SafeIntent intent, String url) {
        final Session session = CustomTabConfig.isCustomTabIntent(intent)
                ? new Session(url, CustomTabConfig.parseCustomTabIntent(context, intent))
                : new Session(source, url);
        addSession(session);
    }

    private void createSession(Context context, Source source, SafeIntent intent, String url, boolean blockingEnabled) {
        final Session session = CustomTabConfig.isCustomTabIntent(intent)
                ? new Session(url, CustomTabConfig.parseCustomTabIntent(context, intent))
                : new Session(source, url);
        session.setBlockingEnabled(blockingEnabled);
        addSession(session);
    }

    private void addSession(Session session) {
        currentSessionUUID = session.getUUID();

        final List<Session> sessions = new ArrayList<>(this.sessions.getValue());
        sessions.add(session);

        this.sessions.setValue(Collections.unmodifiableList(sessions));
    }

    public void selectSession(Session session) {
        if (session.getUUID().equals(currentSessionUUID)) {
            // This is already the selected session.
            return;
        }

        currentSessionUUID = session.getUUID();

        this.sessions.setValue(this.sessions.getValue());
    }

    /**
     * Remove all sessions.
     */
    public void removeAllSessions() {
        currentSessionUUID = null;

        sessions.setValue(Collections.unmodifiableList(Collections.<Session>emptyList()));
    }

    /**
     * Remove the current (selected) session.
     */
    public void removeCurrentSession() {
        removeSession(currentSessionUUID);
    }

    @VisibleForTesting void removeSession(String uuid) {
        final List<Session> sessions = new ArrayList<>();

        int removedFromPosition = -1;

        for (int i = 0; i < this.sessions.getValue().size(); i++) {
            final Session currentSession = this.sessions.getValue().get(i);

            if (currentSession.getUUID().equals(uuid)) {
                removedFromPosition = i;
                continue;
            }

            sessions.add(currentSession);
        }

        if (removedFromPosition == -1) {
            return;
        }

        if (sessions.isEmpty()) {
            currentSessionUUID = null;
        } else {
            final Session currentSession = sessions.get(
                    Math.min(removedFromPosition, sessions.size() - 1));
            currentSessionUUID = currentSession.getUUID();
        }

        this.sessions.setValue(sessions);
    }
}
