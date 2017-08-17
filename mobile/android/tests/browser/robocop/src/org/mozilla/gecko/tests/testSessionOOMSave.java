package org.mozilla.gecko.tests;

import android.util.Log;

import org.mozilla.gecko.Actions;

/**
 * Tests session OOM save behavior.
 *
 * Builds a session and tests that the saved state is correct.
 */
public class testSessionOOMSave extends SessionTest {
    public void testSessionOOMSave() {
        final Actions.EventExpecter pageShowExpecter =
                mActions.expectGlobalEvent(Actions.EventType.UI, "Content:PageShow");
        pageShowExpecter.blockForEvent();
        pageShowExpecter.unregisterListener();

        final Session session = createTestSession(/*selected tab*/ 1);

        // Load the tabs into the browser
        loadSessionTabs(session);

        // Verify sessionstore.js written by Gecko. The session write is delayed
        // to batch successive changes, so the file is repeatedly read until it
        // matches the expected output. Because of the delay, this part of the
        // test takes ~9 seconds to pass.
        VerifyJSONCondition verifyJSONCondition = new VerifyJSONCondition(session);
        boolean success = mSolo.waitForCondition(verifyJSONCondition, SESSION_TIMEOUT);
        if (success) {
            mAsserter.ok(true, "verified session JSON", null);
        } else {
            mAsserter.ok(false, "failed to verify session JSON",
                    Log.getStackTraceString(verifyJSONCondition.getLastException()));
        }
    }
}
