package org.mozilla.gecko.tests;

import org.mozilla.gecko.Actions;

import com.jayway.android.robotium.solo.Condition;

/**
 * Tests session OOM save behavior.
 *
 * Builds a session and tests that the saved state is correct.
 */
public class testSessionOOMSave extends SessionTest {
    private final static int SESSION_TIMEOUT = 25000;

    public void testSessionOOMSave() {
        Actions.EventExpecter pageShowExpecter = mActions.expectGeckoEvent("Content:PageShow");
        pageShowExpecter.blockForEvent();
        pageShowExpecter.unregisterListener();

        PageInfo home = new PageInfo("about:home");
        PageInfo page1 = new PageInfo("page1");
        PageInfo page2 = new PageInfo("page2");
        PageInfo page3 = new PageInfo("page3");
        PageInfo page4 = new PageInfo("page4");
        PageInfo page5 = new PageInfo("page5");
        PageInfo page6 = new PageInfo("page6");

        SessionTab tab1 = new SessionTab(0, home, page1, page2);
        SessionTab tab2 = new SessionTab(1, home, page3, page4);
        SessionTab tab3 = new SessionTab(2, home, page5, page6);

        final Session session = new Session(1, tab1, tab2, tab3);

        // Load the tabs into the browser
        loadSessionTabs(session);

        // Verify sessionstore.js written by Gecko. The session write is
        // delayed for certain interactions (such as changing the selected
        // tab), so the file is repeatedly read until it matches the expected
        // output. Because of the delay, this part of the test takes ~9 seconds
        // to pass.
        VerifyJSONCondition verifyJSONCondition = new VerifyJSONCondition(session);
        boolean success = waitForCondition(verifyJSONCondition, SESSION_TIMEOUT);
        if (success) {
            mAsserter.ok(true, "verified session JSON", null);
        } else {
            mAsserter.ok(false, "failed to verify session JSON",
                    getStackTraceString(verifyJSONCondition.getLastException()));
        }
    }

    private class VerifyJSONCondition implements Condition {
        private AssertException mLastException;
        private final NonFatalAsserter mAsserter = new NonFatalAsserter();
        private final Session mSession;

        public VerifyJSONCondition(Session session) {
            mSession = session;
        }

        @Override
        public boolean isSatisfied() {
            try {
                String sessionString = readProfileFile("sessionstore.js");
                if (sessionString == null) {
                    mLastException = new AssertException("Could not read sessionstore.js");
                    return false;
                }

                verifySessionJSON(mSession, sessionString, mAsserter);
            } catch (AssertException e) {
                mLastException = e;
                return false;
            }
            return true;
        }

        /**
         * Gets the last AssertException thrown by verifySessionJSON().
         *
         * This is useful to get the stack trace if the test fails.
         */
        public AssertException getLastException() {
            return mLastException;
        }
    }
}
