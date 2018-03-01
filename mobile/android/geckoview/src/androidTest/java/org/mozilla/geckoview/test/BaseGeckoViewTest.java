/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview.test;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.mozilla.geckoview.GeckoSession;
import org.mozilla.geckoview.GeckoView;

import android.net.Uri;
import android.os.SystemClock;
import android.support.test.espresso.Espresso;
import android.support.test.espresso.IdlingResource;
import android.support.test.espresso.assertion.ViewAssertions;
import android.support.test.espresso.matcher.ViewMatchers;
import android.support.test.rule.ActivityTestRule;
import android.view.MotionEvent;

import static org.junit.Assert.assertTrue;

public class BaseGeckoViewTest {
    protected GeckoSession mSession;
    protected GeckoView mView;
    private ExplicitIdlingResource mIdlingResource;

    @Before
    public void setup() {
        mView = mActivityRule.getActivity().getGeckoView();
        mSession = mActivityRule.getActivity().getGeckoSession();

        mIdlingResource = new ExplicitIdlingResource();
        Espresso.registerIdlingResources(mIdlingResource);
    }

    @After
    public void tearDown() {
        Espresso.unregisterIdlingResources(mIdlingResource);
    }

    @Rule
    public ActivityTestRule<TestRunnerActivity> mActivityRule = new ActivityTestRule<>(
            TestRunnerActivity.class);

    protected void waitUntilDone() {
        Espresso.onView(ViewMatchers.isRoot()).check(ViewAssertions.matches(ViewMatchers.isRoot()));
    }

    protected void done() {
        mIdlingResource.done();
    }

    protected String buildAssetUrl(String path) {
        return "resource://android/assets/www/" + path;
    }

    protected void loadTestPath(final String path, Runnable finished) {
        loadTestPage(buildAssetUrl(path), finished);
    }

    protected void loadTestPage(final String url, final Runnable finished) {
        final String path = Uri.parse(url).getPath();
        mSession.setProgressDelegate(new GeckoSession.ProgressDelegate() {
            @Override
            public void onPageStart(GeckoSession session, String loadingUrl) {
                assertTrue("Loaded url should end with " + path, loadingUrl.endsWith(path));
            }

            @Override
            public void onPageStop(GeckoSession session, boolean success) {
                assertTrue("Load should succeed", success);
                mSession.setProgressDelegate(null);
                finished.run();
            }

            @Override
            public void onSecurityChange(GeckoSession session, SecurityInformation securityInfo) {
            }
        });

        mSession.loadUri(url);
    }

    protected void sendClick(int x, int y) {
        mSession.getPanZoomController().onTouchEvent(MotionEvent.obtain(SystemClock.uptimeMillis(), SystemClock.uptimeMillis(), MotionEvent.ACTION_DOWN, 5, 5, 0));
        mSession.getPanZoomController().onTouchEvent(MotionEvent.obtain(SystemClock.uptimeMillis(), SystemClock.uptimeMillis(), MotionEvent.ACTION_UP, 5, 5, 0));
    }

    private class ExplicitIdlingResource implements IdlingResource {
        private boolean mIsIdle;
        private ResourceCallback mCallback;

        @Override
        public String getName() {
            return "Explicit Idling Resource";
        }

        @Override
        public boolean isIdleNow() {
            return mIsIdle;
        }

        public void done() {
            mIsIdle = true;
            if (mCallback != null) {
                mCallback.onTransitionToIdle();
            }
        }

        @Override
        public void registerIdleTransitionCallback(ResourceCallback callback) {
            mCallback = callback;
        }
    }
}
