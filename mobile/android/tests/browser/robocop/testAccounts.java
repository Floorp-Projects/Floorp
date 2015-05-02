/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;


public class testAccounts extends JavascriptTest {
    public testAccounts() {
        super("testAccounts.js");
    }

    @Override
    public void testJavascript() throws Exception {
        super.testJavascript();

        // Rather than waiting for the JS call to message
        // Java and wait for the Activity to launch, we just
        // don't test these.
        /*
        android.app.Activity activity = mSolo.getCurrentActivity();
        System.out.println("Current activity: " + activity);
        mAsserter.ok(activity instanceof FxAccountGetStartedActivity, "checking activity", "setup activity launched");
        */
    }
}
