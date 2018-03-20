/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.session;

import android.content.Intent;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.support.customtabs.CustomTabsIntent;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.focus.customtabs.CustomTabConfig;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

import java.util.List;
import java.util.UUID;

import mozilla.components.utils.SafeIntent;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

@RunWith(RobolectricTestRunner.class)
public class SessionManagerTest {
    private static final String TEST_URL = "https://github.com/mozilla-mobile/focus-android";
    private static final String TEST_URL_2 = "https://www.mozilla.org";
    private static final String TEST_URL_3 = "https://www.mozilla.org/en-US/firefox/focus/";

    @Before
    public void setUp() {
        // Always start tests with a clean session manager
        SessionManager sessionManager = SessionManager.getInstance();

        sessionManager.removeAllSessions();
        sessionManager.removeAllCustomTabSessions();
    }

    @Test
    public void testInitialState() {
        final SessionManager sessionManager = SessionManager.getInstance();

        assertNotNull(sessionManager.getSessions().getValue());
        assertEquals(0, sessionManager.getSessions().getValue().size());
        assertFalse(sessionManager.hasSession());
    }

    @Test
    public void testViewIntent() {
        final SessionManager sessionManager = SessionManager.getInstance();

        final SafeIntent intent = new SafeIntent(new Intent(Intent.ACTION_VIEW, Uri.parse(TEST_URL)));
        sessionManager.handleIntent(RuntimeEnvironment.application, intent, null);

        final List<Session> sessions = sessionManager.getSessions().getValue();
        assertNotNull(sessions);
        assertEquals(1, sessions.size());

        final Session session = sessions.get(0);
        assertEquals(TEST_URL, session.getUrl().getValue());
        assertFalse(session.isCustomTab());
        assertNull(session.getCustomTabConfig());

        assertTrue(sessionManager.hasSession());
    }

    /**
     * In production we see apps send VIEW intents without an URL. (#1373)
     */
    @Test
    public void testViewIntentWithNullURL() {
        final SessionManager sessionManager = SessionManager.getInstance();
        assertFalse(sessionManager.hasSession());

        final SafeIntent intent = new SafeIntent(new Intent(Intent.ACTION_VIEW, null));
        sessionManager.handleIntent(RuntimeEnvironment.application, intent, null);

        // We ignored the Intent and no session has been created.
        assertFalse(sessionManager.hasSession());
    }

    @Test
    public void testCustomTabIntent() {
        final SessionManager sessionManager = SessionManager.getInstance();

        final SafeIntent intent = new SafeIntent(new CustomTabsIntent.Builder()
                .setToolbarColor(Color.GREEN)
                .addDefaultShareMenuItem()
                .build()
                .intent
                .setData(Uri.parse(TEST_URL))
                .putExtra(CustomTabConfig.EXTRA_CUSTOM_TAB_ID, UUID.randomUUID().toString()));

        sessionManager.handleIntent(RuntimeEnvironment.application, intent, null);

        final List<Session> sessions = sessionManager.getSessions().getValue();
        assertNotNull(sessions);
        assertEquals(0, sessions.size());

        final List<Session> customTabSessions = sessionManager.getCustomTabSessions().getValue();
        assertNotNull(customTabSessions);
        assertEquals(1, customTabSessions.size());

        final Session session = customTabSessions.get(0);
        assertEquals(TEST_URL, session.getUrl().getValue());
        assertTrue(session.isCustomTab());
        assertNotNull(session.getCustomTabConfig());

        final CustomTabConfig config = session.getCustomTabConfig();
        assertNotNull(config.toolbarColor);
        assertEquals(Color.GREEN, config.toolbarColor.intValue());
        assertTrue(config.showShareMenuItem);

        assertFalse(sessionManager.hasSession());
    }

    @Test
    public void testViewIntentFromHistoryIsIgnored() {
        final SessionManager sessionManager = SessionManager.getInstance();

        final Intent unsafeIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(TEST_URL));
        unsafeIntent.addFlags(Intent.FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY);

        final SafeIntent intent = new SafeIntent(unsafeIntent);
        sessionManager.handleIntent(RuntimeEnvironment.application, intent, null);

        final List<Session> sessions = sessionManager.getSessions().getValue();
        assertNotNull(sessions);
        assertEquals(0, sessions.size());

        assertFalse(sessionManager.hasSession());
    }

    @Test
    public void testNoSessionIsCreatedIfWeAreRestoring() {
        final SessionManager sessionManager = SessionManager.getInstance();

        final Bundle instanceState = new Bundle();

        final SafeIntent intent = new SafeIntent(new Intent(Intent.ACTION_VIEW, Uri.parse(TEST_URL)));
        sessionManager.handleIntent(RuntimeEnvironment.application, intent, instanceState);

        final List<Session> sessions = sessionManager.getSessions().getValue();
        assertNotNull(sessions);
        assertEquals(0, sessions.size());

        assertFalse(sessionManager.hasSession());
    }

    @Test
    public void testShareIntentViaNewIntent() {
        final SessionManager sessionManager = SessionManager.getInstance();

        final Intent unsafeIntent = new Intent(Intent.ACTION_SEND);
        unsafeIntent.putExtra(Intent.EXTRA_TEXT, TEST_URL);

        sessionManager.handleNewIntent(RuntimeEnvironment.application, new SafeIntent(unsafeIntent));

        final List<Session> sessions = sessionManager.getSessions().getValue();
        assertNotNull(sessions);
        assertEquals(1, sessions.size());

        final Session session = sessions.get(0);
        assertFalse(session.isSearch());
        assertEquals(TEST_URL, session.getUrl().getValue());
        assertFalse(session.isCustomTab());
        assertNull(session.getCustomTabConfig());

        assertTrue(sessionManager.hasSession());
    }

    public void testShareIntentWithTextViaNewIntent() {
        final SessionManager sessionManager = SessionManager.getInstance();

        final Intent unsafeIntent = new Intent(Intent.ACTION_SEND);
        unsafeIntent.putExtra(Intent.EXTRA_TEXT, "Hello World Focus");

        sessionManager.handleNewIntent(RuntimeEnvironment.application, new SafeIntent(unsafeIntent));

        final List<Session> sessions = sessionManager.getSessions().getValue();
        assertNotNull(sessions);
        assertEquals(1, sessions.size());

        final Session session = sessions.get(0);
        assertTrue(session.isSearch());
        assertEquals("Hello World Focus", session.getSearchTerms());
        assertEquals(TEST_URL, session.getUrl().getValue());
        assertFalse(session.isCustomTab());
        assertNull(session.getCustomTabConfig());

        assertTrue(sessionManager.hasSession());
    }

    @Test(expected = IllegalAccessError.class)
    public void getCurrentSessionThrowsExceptionIfThereIsNoSession() {
        final SessionManager sessionManager = SessionManager.getInstance();
        sessionManager.getCurrentSession();
    }

    @Test
    public void testHasSessionWithUUID() {
        final SessionManager sessionManager = SessionManager.getInstance();
        assertFalse(sessionManager.hasSessionWithUUID(UUID.randomUUID().toString()));

        sessionManager.createSession(Source.USER_ENTERED, TEST_URL);

        assertTrue(sessionManager.hasSession());

        final Session session = sessionManager.getCurrentSession();
        assertNotNull(session);
        assertTrue(sessionManager.hasSessionWithUUID(session.getUUID()));
        assertNotNull(sessionManager.getSessionByUUID(session.getUUID()));
    }

    @Test(expected = IllegalAccessError.class)
    public void getSessionByUUIDThrowsExceptionIfSessionDoesNotExist() {
        final SessionManager sessionManager = SessionManager.getInstance();
        sessionManager.getSessionByUUID(UUID.randomUUID().toString());
    }

    @Test
    public void testRemovingSessions() {
        final SessionManager sessionManager = SessionManager.getInstance();

        sessionManager.createSession(Source.USER_ENTERED, TEST_URL);
        sessionManager.createSession(Source.VIEW, TEST_URL_2);
        sessionManager.createSession(Source.SHARE, TEST_URL_3);

        {
            final List<Session> sessions = sessionManager.getSessions().getValue();
            assertEquals(3, sessions.size());
        }

        {
            final Session currentSession = sessionManager.getCurrentSession();
            assertEquals(Source.SHARE, currentSession.getSource());
            assertEquals(TEST_URL_3, currentSession.getUrl().getValue());
        }

        sessionManager.removeCurrentSession();

        {
            final Session currentSession = sessionManager.getCurrentSession();
            assertEquals(Source.VIEW, currentSession.getSource());
            assertEquals(TEST_URL_2, currentSession.getUrl().getValue());
        }

        sessionManager.removeCurrentSession();

        {
            final Session currentSession = sessionManager.getCurrentSession();
            assertEquals(Source.USER_ENTERED, currentSession.getSource());
            assertEquals(TEST_URL, currentSession.getUrl().getValue());
        }

        sessionManager.removeCurrentSession();

        assertFalse(sessionManager.hasSession());
        assertEquals(0, sessionManager.getSessions().getValue().size());
    }

    @Test
    public void testRemovingAllSessions() {
        final SessionManager sessionManager = SessionManager.getInstance();

        sessionManager.createSession(Source.USER_ENTERED, TEST_URL);
        sessionManager.createSession(Source.VIEW, TEST_URL_2);
        sessionManager.createSession(Source.SHARE, TEST_URL_3);

        assertTrue(sessionManager.hasSession());
        assertEquals(3, sessionManager.getSessions().getValue().size());

        sessionManager.removeAllSessions();

        assertFalse(sessionManager.hasSession());
        assertEquals(0, sessionManager.getSessions().getValue().size());
    }

    @Test
    public void testHasSessionWithUUIDWithUnknownUUID() {
        final SessionManager sessionManager = SessionManager.getInstance();
        sessionManager.createSession(Source.USER_ENTERED, TEST_URL);

        assertFalse(sessionManager.hasSessionWithUUID(UUID.randomUUID().toString()));
    }

    @Test
    public void testRemovingUnknownSessionHasNoEffect() {
        final SessionManager sessionManager = SessionManager.getInstance();
        sessionManager.removeRegularSession(UUID.randomUUID().toString());

        assertFalse(sessionManager.hasSession());
        assertEquals(0, sessionManager.getSessions().getValue().size());
    }

    @Test
    public void testSeparateCustomTabSessions() {
        final String customTabId = "342d6440-760a-43ba-86c4-fe896eb20a29";

        final SessionManager sessionManager = SessionManager.getInstance();

        assertFalse(sessionManager.hasSession());
        assertEquals(0, sessionManager.getCustomTabSessions().getValue().size());

        final SafeIntent intent = new SafeIntent(new CustomTabsIntent.Builder()
                .build()
                .intent
                .setData(Uri.parse(TEST_URL))
                .putExtra(CustomTabConfig.EXTRA_CUSTOM_TAB_ID, customTabId));

        sessionManager.handleIntent(RuntimeEnvironment.application, intent, null);

        assertFalse(sessionManager.hasSession());
        assertEquals(1, sessionManager.getCustomTabSessions().getValue().size());

        assertNotNull(sessionManager.getCustomTabSessionByCustomTabId(customTabId));
    }

    @Test(expected = IllegalAccessError.class)
    public void testThrowsIfCustomTabDoesNotExist() {
        final String customTabId = "342d6440-760a-43ba-86c4-fe896eb20a29";

        final SessionManager sessionManager = SessionManager.getInstance();
        sessionManager.getCustomTabSessionByCustomTabIdOrThrow(customTabId);
    }

    @Test
    public void testNullReturnedIfCustomTabDoesNotExist() {
        final String customTabId = "342d6440-760a-43ba-86c4-fe896eb20a29";

        final SessionManager sessionManager = SessionManager.getInstance();
        assertNull(sessionManager.getCustomTabSessionByCustomTabId(customTabId));
    }

    @Test
    public void testTransformingCustomTabIntoRegularSession() {
        final String customTabId = "342d6440-760a-43ba-86c4-fe896eb20a29";

        final SessionManager sessionManager = SessionManager.getInstance();

        assertFalse(sessionManager.hasSession());
        assertEquals(0, sessionManager.getCustomTabSessions().getValue().size());
        assertEquals(0, sessionManager.getSessions().getValue().size());

        final SafeIntent intent = new SafeIntent(new CustomTabsIntent.Builder()
                .build()
                .intent
                .setData(Uri.parse(TEST_URL))
                .putExtra(CustomTabConfig.EXTRA_CUSTOM_TAB_ID, customTabId));

        sessionManager.handleIntent(RuntimeEnvironment.application, intent, null);

        assertFalse(sessionManager.hasSession());
        assertEquals(1, sessionManager.getCustomTabSessions().getValue().size());
        assertEquals(0, sessionManager.getSessions().getValue().size());

        final Session session = sessionManager.getCustomTabSessionByCustomTabId(customTabId);
        assertNotNull(session);
        assertTrue(session.isCustomTab());
        assertEquals(Source.CUSTOM_TAB, session.getSource());
        assertNotNull(session.getCustomTabConfig());

        sessionManager.moveCustomTabToRegularSessions(session);

        assertTrue(sessionManager.hasSession());
        assertEquals(0, sessionManager.getCustomTabSessions().getValue().size());
        assertEquals(1, sessionManager.getSessions().getValue().size());

        assertFalse(session.isCustomTab());
        assertEquals(Source.NONE, session.getSource());
        assertNull(session.getCustomTabConfig());
    }
}