/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.shortcut;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import static org.junit.Assert.assertEquals;

@RunWith(RobolectricTestRunner.class)
public class HomeScreenTest {
    @Test
    public void testGenerateTitleFromUrl() {
        assertEquals("mozilla.org", HomeScreen.generateTitleFromUrl("https://www.mozilla.org"));
        assertEquals("facebook.com", HomeScreen.generateTitleFromUrl("http://m.facebook.com/home"));
    }
}