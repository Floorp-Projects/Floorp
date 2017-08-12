package org.mozilla.gecko.tests;

import android.content.Intent;

import org.mozilla.gecko.tests.helpers.GeckoHelper;

/**
 * Tests session OOM restore behavior.
 *
 * Loads a session and tests that it is restored correctly.
 */
public class testSessionOOMRestore extends SessionTest {
    private Session mSession;

    @Override
    public void setActivityIntent(Intent intent) {
        mSession = createTestSession(/*selected tab*/ 1);

        injectSessionToRestore(intent, mSession);

        super.setActivityIntent(intent);
    }

    public void testSessionOOMRestore() throws Exception {
        GeckoHelper.blockForReady();
        verifySessionTabs(mSession);
    }
}
