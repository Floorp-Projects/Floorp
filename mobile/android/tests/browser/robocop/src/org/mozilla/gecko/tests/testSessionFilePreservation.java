package org.mozilla.gecko.tests;

import android.content.Intent;
import android.util.Log;

import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.tests.helpers.GeckoHelper;

/**
 * Tests session OOM save behavior.
 *
 * Builds a session on disk, restores it and tests that
 * it is then written back to disk correctly.
 */
public class testSessionFilePreservation extends SessionTest {
    private Session mSession;

    @Override
    public void setActivityIntent(Intent intent) {
        mSession = createTestSession(/*selected tab*/ 2);

        injectSessionToRestore(intent, mSession);

        super.setActivityIntent(intent);
    }

    public void testSessionFilePreservation() {
        GeckoHelper.blockForReady();

        // We have tests for making sure that
        // - tabs are correctly restored from a session store file
        // - opening new tabs generates a correct session store file

        // These tests only use live tabs that are fully loaded. We now also want
        // to test that zombie tabs that have been unloaded in background are
        // written correctly as well, especially those zombie tabs that have been
        // created directly from the session file during session restoring.
        // Therefore we don't touch any of those tabs here to make sure that we're
        // not accidentally reloading them.

        // To make sure we're not simply verifying the very same file we've generated
        // and written ourselves further up, we delete it...
        deleteProfileFile("sessionstore.js");

        // ... and then trigger some session store activity to make sure
        // that a fresh session file is written.
        Tab tab = Tabs.getInstance().addTab();
        Tabs.getInstance().closeTab(tab);

        // Verify sessionstore.js written by Gecko. The session write is delayed
        // to batch successive changes, so the file is repeatedly read until it
        // matches the expected output. Because of the delay, this part of the
        // test takes ~9 seconds to pass.
        VerifyJSONCondition verifyJSONCondition = new VerifyJSONCondition(mSession);
        boolean success = mSolo.waitForCondition(verifyJSONCondition, SESSION_TIMEOUT);
        if (success) {
            mAsserter.ok(true, "verified session JSON", null);
        } else {
            mAsserter.ok(false, "failed to verify session JSON",
                    Log.getStackTraceString(verifyJSONCondition.getLastException()));
        }
    }
}
