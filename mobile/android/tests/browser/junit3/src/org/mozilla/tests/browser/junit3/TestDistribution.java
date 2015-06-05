/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.tests.browser.junit3;

import android.test.InstrumentationTestCase;
import org.mozilla.gecko.distribution.ReferrerDescriptor;

public class TestDistribution extends InstrumentationTestCase {
    private static final String TEST_REFERRER_STRING = "utm_source=campsource&utm_medium=campmed&utm_term=term%2Bhere&utm_content=content&utm_campaign=name";
    private static final String TEST_MALFORMED_REFERRER_STRING = "utm_source=campsource&utm_medium=campmed&utm_term=term%2";

    public void testReferrerParsing() {
        ReferrerDescriptor good = new ReferrerDescriptor(TEST_REFERRER_STRING);
        assertEquals("campsource", good.source);
        assertEquals("campmed", good.medium);
        assertEquals("term+here", good.term);
        assertEquals("content", good.content);
        assertEquals("name", good.campaign);

        // Uri.Builder is permissive.
        ReferrerDescriptor bad = new ReferrerDescriptor(TEST_MALFORMED_REFERRER_STRING);
        assertEquals("campsource", bad.source);
        assertEquals("campmed", bad.medium);
        assertFalse("term+here".equals(bad.term));
        assertNull(bad.content);
        assertNull(bad.campaign);

        ReferrerDescriptor ugly = new ReferrerDescriptor(null);
        assertNull(ugly.source);
        assertNull(ugly.medium);
        assertNull(ugly.term);
        assertNull(ugly.content);
        assertNull(ugly.campaign);
    }
}
