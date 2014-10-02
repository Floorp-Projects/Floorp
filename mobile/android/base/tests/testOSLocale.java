/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import org.mozilla.gecko.Actions;

public class testOSLocale extends JavascriptTest {
    private static final String GECKO_DELAYED_STARTUP = "Gecko:DelayedStartup";

    public testOSLocale() {
        super("testOSLocale.js");
    }

    @Override
    public void testJavascript() throws Exception {
        final Actions.EventExpecter expecter = mActions.expectGeckoEvent(GECKO_DELAYED_STARTUP);
        expecter.blockForEvent(MAX_WAIT_MS, true);

        // We wait for Gecko:DelayedStartup above, so we *must not* wait for Gecko:Ready.
        // Call doTestJavascript directly.
        super.doTestJavascript();
    }
}
