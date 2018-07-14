/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.search.parser

import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class SuggestionParserTest {
    @Test
    fun `Parse a search suggestion result`() {
        val json = "[\"firefox\",[\"firefox\",\"firefox for mac\",\"firefox quantum\",\"firefox update\",\"firefox esr\",\"firefox focus\",\"firefox addons\",\"firefox extensions\",\"firefox nightly\",\"firefox clear cache\"]]"

        val results = googleParser(json)
        val expectedResults = listOf("firefox", "firefox for mac", "firefox quantum", "firefox update", "firefox esr", "firefox focus", "firefox addons", "firefox extensions", "firefox nightly", "firefox clear cache")
        assertEquals(expectedResults, results)
    }
}
