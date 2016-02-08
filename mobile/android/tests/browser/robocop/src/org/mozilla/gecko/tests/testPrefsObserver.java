/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import org.mozilla.gecko.Actions;

/**
 * Basic test to check bounce-back from overscroll.
 * - Load the page and verify it draws
 * - Drag page downwards by 100 pixels into overscroll, verify it snaps back.
 * - Drag page rightwards by 100 pixels into overscroll, verify it snaps back.
 */
public class testPrefsObserver extends BaseTest {
    private static final String PREF_TEST_PREF = "robocop.tests.dummy";

    private Actions.PrefWaiter prefWaiter;
    private boolean prefValue;

    public void setPref(boolean value) {
        mAsserter.dumpLog("Setting pref");
        mActions.setPref(PREF_TEST_PREF, value, /* flush */ false);
    }

    public void waitAndCheckPref(boolean value) {
        mAsserter.dumpLog("Waiting to check pref");

        mAsserter.isnot(prefWaiter, null, "Check pref waiter is not null");
        prefWaiter.waitForFinish();

        mAsserter.is(prefValue, value, "Check correct pref value");
    }

    public void verifyDisconnect() {
        mAsserter.dumpLog("Checking pref observer is removed");

        final boolean newValue = !prefValue;
        setPreferenceAndWaitForChange(PREF_TEST_PREF, newValue);
        mAsserter.isnot(prefValue, newValue, "Check pref value did not change");
    }

    public void observePref() {
        mAsserter.dumpLog("Setting up pref observer");

        // Setup the pref observer
        mAsserter.is(prefWaiter, null, "Check pref waiter is null");
        prefWaiter = mActions.addPrefsObserver(
                new String[] { PREF_TEST_PREF }, new Actions.PrefHandlerBase() {
            @Override // Actions.PrefHandlerBase
            public void prefValue(String pref, boolean value) {
                mAsserter.is(pref, PREF_TEST_PREF, "Check correct pref name");
                prefValue = value;
            }
        });
    }

    public void removePrefObserver() {
        mAsserter.dumpLog("Removing pref observer");

        mActions.removePrefsObserver(prefWaiter);
    }

    public void testPrefsObserver() {
        blockForGeckoReady();

        setPref(false);
        observePref();
        waitAndCheckPref(false);

        setPref(true);
        waitAndCheckPref(true);

        removePrefObserver();
        verifyDisconnect();

        // Removing again should be a no-op.
        removePrefObserver();
    }
}

