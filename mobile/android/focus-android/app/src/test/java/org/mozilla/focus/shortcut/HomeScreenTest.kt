/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.shortcut

import junit.framework.TestCase.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.shortcut.HomeScreen.generateTitleFromUrl
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class HomeScreenTest {
    @Test
    fun testGenerateTitleFromUrl() {
        assertEquals("mozilla.org", generateTitleFromUrl("https://www.mozilla.org"))
        assertEquals("facebook.com", generateTitleFromUrl("http://m.facebook.com/home"))
        assertEquals("", generateTitleFromUrl("mozilla"))
    }
}
