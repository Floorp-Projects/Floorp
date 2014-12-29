/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import org.mozilla.gecko.AppConstants;

public class testAboutPasswords extends JavascriptTest {
    private static final String LOGTAG = testAboutPasswords.class.getSimpleName();

    public testAboutPasswords() {
        super("testAboutPasswords.js");
    }

    @Override
    public void testJavascript() throws Exception {
        // This feature is only available in Nightly.
        if (!AppConstants.NIGHTLY_BUILD) {
            mAsserter.dumpLog(LOGTAG + " is disabled on non-Nightly builds: returning");
            return;
        }
        super.testJavascript();
    }
}
