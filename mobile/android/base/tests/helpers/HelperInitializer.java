/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests.helpers;

import org.mozilla.gecko.tests.UITestContext;

/**
 * AssertionHelper is statically imported in many places. Thus we want to hide
 * its init method outside of this package. We initialize the remaining helper
 * classes from here so that all the init methods are package protected.
 */
public final class HelperInitializer {

    private HelperInitializer() { /* To disallow instantiation. */ }

    public static void init(final UITestContext context) {
        // Other helpers make assertions so init AssertionHelper first.
        AssertionHelper.init(context);

        DeviceHelper.init(context);
        GeckoHelper.init(context);
        JavascriptBridge.init(context);
        NavigationHelper.init(context);
        WaitHelper.init(context);
    }
}
