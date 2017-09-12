/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.session;

import android.arch.lifecycle.Lifecycle;
import android.arch.lifecycle.LifecycleOwner;
import android.arch.lifecycle.Observer;
import android.graphics.Color;
import android.os.Bundle;
import android.support.customtabs.CustomTabsIntent;
import android.text.TextUtils;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.focus.customtabs.CustomTabConfig;
import org.mozilla.focus.utils.SafeIntent;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

@RunWith(RobolectricTestRunner.class)
public class SessionTest {
    private static final String TEST_URL = "https://www.mozilla.org";
    private static final String TEST_URL_2 = "https://www.example.org";

    @Test
    public void testEmptySession() {
        final Session session = new Session(Source.VIEW, TEST_URL);

        assertFalse(TextUtils.isEmpty(session.getUUID()));
        assertEquals(TEST_URL, session.getUrl().getValue());

        assertEquals(0, (int) session.getProgress().getValue());
        assertFalse(session.getSecure().getValue());
        assertFalse(session.getLoading().getValue());
        assertEquals(0, (int) session.getBlockedTrackers().getValue());

        assertNull(session.getCustomTabConfig());
        assertFalse(session.isCustomTab());
    }

    @Test
    public void testObserversGetInitialValues() {
        final Session session = new Session(Source.VIEW, TEST_URL);

        {
            final Observer<String> urlObserver = mockObserver();
            session.getUrl().observe(mockLifeCycleOwner(), urlObserver);
            verify(urlObserver).onChanged(TEST_URL);
        }
        {
            final Observer<Integer> progressObserver = mockObserver();
            session.getProgress().observe(mockLifeCycleOwner(), progressObserver);
            verify(progressObserver).onChanged(0);
        }
        {
            final Observer<Boolean> secureObserver = mockObserver();
            session.getSecure().observe(mockLifeCycleOwner(), secureObserver);
            verify(secureObserver).onChanged(false);
        }
        {
            final Observer<Boolean> loadingObserver = mockObserver();
            session.getLoading().observe(mockLifeCycleOwner(), loadingObserver);
            verify(loadingObserver).onChanged(false);
        }
        {
            final Observer<Integer> blockedTrackersObserver = mockObserver();
            session.getBlockedTrackers().observe(mockLifeCycleOwner(), blockedTrackersObserver);
            verify(blockedTrackersObserver).onChanged(0);
        }
    }

    @Test
    public void testObservingValueChanges() {
        final Session session = new Session(Source.VIEW, TEST_URL);

        {
            final Observer<String> urlObserver = mockObserver();
            session.getUrl().observe(mockLifeCycleOwner(), urlObserver);
            verify(urlObserver).onChanged(TEST_URL);

            session.setUrl(TEST_URL_2);
            verify(urlObserver).onChanged(TEST_URL_2);
        }
        {
            final Observer<Integer> progressObserver = mockObserver();
            session.getProgress().observe(mockLifeCycleOwner(), progressObserver);
            verify(progressObserver).onChanged(0);

            session.setProgress(42);
            verify(progressObserver).onChanged(42);
        }
        {
            final Observer<Boolean> secureObserver = mockObserver();
            session.getSecure().observe(mockLifeCycleOwner(), secureObserver);
            verify(secureObserver).onChanged(false);

            session.setSecure(true);
            verify(secureObserver).onChanged(true);
        }
        {
            final Observer<Boolean> loadingObserver = mockObserver();
            session.getLoading().observe(mockLifeCycleOwner(), loadingObserver);
            verify(loadingObserver).onChanged(false);

            session.setLoading(true);
            verify(loadingObserver).onChanged(true);
        }
        {
            final Observer<Integer> blockedTrackersObserver = mockObserver();
            session.getBlockedTrackers().observe(mockLifeCycleOwner(), blockedTrackersObserver);
            verify(blockedTrackersObserver).onChanged(0);

            session.setTrackersBlocked(23);
            verify(blockedTrackersObserver).onChanged(23);
        }
    }

    @Test
    public void testCustomTabConfiguration() {
        final CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();
        builder.setToolbarColor(Color.GREEN);

        final SafeIntent intent = new SafeIntent(builder.build().intent);
        final CustomTabConfig customTabConfig = CustomTabConfig.parseCustomTabIntent(RuntimeEnvironment.application, intent);
        final Session session = new Session(TEST_URL, customTabConfig);

        assertTrue(session.isCustomTab());
        assertNotNull(session.getCustomTabConfig());
        assertNotNull(session.getCustomTabConfig().toolbarColor);
        assertEquals(Color.GREEN, (int) session.getCustomTabConfig().toolbarColor);
    }

    @Test
    public void testSavingAndRetrievingWebViewState() {
        final Session session = new Session(Source.VIEW, TEST_URL);

        assertNull(session.getWebViewState());

        {
            final Bundle bundle = new Bundle();
            bundle.putInt("friday", 13);

            session.saveWebViewState(bundle);
        }

        assertNotNull(session.getWebViewState());

        {
            final Bundle bundle = session.getWebViewState();
            assertTrue(bundle.containsKey("friday"));
            assertEquals(13, bundle.getInt("friday"));
        }
    }

    @Test
    public void testIsSearch() {
        final Session session = new Session(Source.VIEW, TEST_URL);
        assertFalse(session.isSearch());

        session.setSearchTerms("hello world");
        assertTrue(session.isSearch());

        session.clearSearchTerms();
        assertFalse(session.isSearch());

        session.setSearchTerms(null);
        assertFalse(session.isSearch());

        session.setSearchTerms("");
        assertFalse(session.isSearch());
    }

    @Test
    public void testIsRecorded() {
        final Session session = new Session(Source.VIEW, TEST_URL);
        assertFalse(session.isRecorded());

        session.markAsRecorded();
        assertTrue(session.isRecorded());
    }

    @Test
    public void testSameAs() {
        final Session session1 = new Session(Source.VIEW, TEST_URL);
        final Session session2 = new Session(Source.VIEW, TEST_URL);

        assertFalse(session1.isSameAs(session2));
        assertFalse(session2.isSameAs(session1));

        assertTrue(session1.isSameAs(session1));
        assertTrue(session2.isSameAs(session2));
    }

    @SuppressWarnings("unchecked")
    private <T> Observer<T> mockObserver() {
        return mock(Observer.class);
    }

    private LifecycleOwner mockLifeCycleOwner() {
        final Lifecycle lifecycle = mock(Lifecycle.class);
        doReturn(Lifecycle.State.RESUMED).when(lifecycle).getCurrentState();

        final LifecycleOwner lifecycleOwner = mock(LifecycleOwner.class);
        doReturn(lifecycle).when(lifecycleOwner).getLifecycle();

        return lifecycleOwner;
    }
}