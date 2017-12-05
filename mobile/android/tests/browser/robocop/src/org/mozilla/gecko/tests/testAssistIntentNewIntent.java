package org.mozilla.gecko.tests;

import android.content.Context;
import android.content.Intent;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.tests.helpers.GeckoHelper;
import org.mozilla.gecko.tests.helpers.WaitHelper;

public class testAssistIntentNewIntent extends AssistIntentTest {
    public void testAssistIntentNewIntent() {
        GeckoHelper.blockForReady();

        final Context testContext = getInstrumentation().getContext();
        final Intent assistIntent = new Intent(Intent.ACTION_ASSIST);
        assistIntent.setPackage(AppConstants.ANDROID_PACKAGE_NAME);
        assistIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        WaitHelper.waitForPageLoad(new Runnable() {
            @Override
            public void run() {
                testContext.startActivity(assistIntent);
            }
        });

        verifyAssistIntentHandling();
    }
}
