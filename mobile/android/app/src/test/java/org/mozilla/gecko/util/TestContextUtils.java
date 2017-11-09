/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.util;

import android.content.Context;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.robolectric.RuntimeEnvironment;

import static org.junit.Assert.*;

/**
 * Unit test methods of the ContextUtils class.
 */
@RunWith(TestRunner.class)
public class TestContextUtils {

    private Context context;

    @Before
    public void setUp() {
        context = RuntimeEnvironment.application;
    }

    @Test
    public void testGetPackageInstallTimeReturnsReasonableValue() throws Exception {
        // At the time of writing, Robolectric's value is 0, which is reasonable.
        final long installTime = ContextUtils.getCurrentPackageInfo(context).firstInstallTime;
        assertTrue("Package install time is positive", installTime >= 0);
        assertTrue("Package install time is less than current time", installTime < System.currentTimeMillis());
    }
}
