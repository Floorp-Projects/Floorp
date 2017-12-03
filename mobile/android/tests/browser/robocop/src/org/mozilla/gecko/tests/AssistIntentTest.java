package org.mozilla.gecko.tests;

import android.content.Intent;

import org.mozilla.gecko.tests.helpers.GeckoHelper;

public class AssistIntentTest extends SessionTest {
    @Override
    public void setActivityIntent(Intent intent) {
        // We want to make sure that we have opened a new tab, so we're creating a session
        // where the default selected tab will *not* be about:home.
        Session session = createTestSession(/*selected tab*/ 1);
        injectSessionToRestore(intent, session);

        super.setActivityIntent(intent);
    }

    protected void verifyAssistIntentHandling() {
        // After an ACTION_ASSIST intent, we should be in editing mode...
        mToolbar.assertIsEditing();

        // ... the input field should be empty in readiness for the user to type...
        mToolbar.assertUrl("");

        // ... and we should have opened a new tab in addition to those from the "restored" session.
        mToolbar.dismissEditingMode();
        mToolbar.assertTitle(mStringHelper.ABOUT_HOME_URL);
    }
}
