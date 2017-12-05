package org.mozilla.gecko.tests;

import android.content.Intent;

import org.mozilla.gecko.tests.helpers.GeckoHelper;

public class testAssistIntentStartup extends AssistIntentTest {
    @Override
    public void setActivityIntent(Intent intent) {
        intent.setAction(Intent.ACTION_ASSIST);

        super.setActivityIntent(intent);
    }

    public void testAssistIntentStartup() {
        GeckoHelper.blockForReady();

        verifyAssistIntentHandling();
    }
}
