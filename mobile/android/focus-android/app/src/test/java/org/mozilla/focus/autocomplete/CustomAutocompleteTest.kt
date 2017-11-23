/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.autocomplete

import android.content.Context
import kotlinx.coroutines.experimental.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

@RunWith(RobolectricTestRunner::class)
class CustomAutocompleteTest {
    @Before
    fun setUp() {
        RuntimeEnvironment.application
                .getSharedPreferences("custom_autocomplete", Context.MODE_PRIVATE)
                .edit()
                .clear()
                .apply()
    }

    @Test
    fun testCustomListIsEmptyByDefault() {
        val domains = runBlocking {
            CustomAutocomplete.loadCustomAutoCompleteDomains(RuntimeEnvironment.application)
        }

        assertEquals(0, domains.size)
    }

    @Test
    fun testSavingAndLoadingDomains() = runBlocking {
        CustomAutocomplete.saveDomains(RuntimeEnvironment.application, setOf(
                "mozilla.org",
                "example.org",
                "example.com"
        ))

        val domains = CustomAutocomplete.loadCustomAutoCompleteDomains(RuntimeEnvironment.application)

        assertEquals(3, domains.size)
        assertEquals("mozilla.org", domains.elementAt(0))
        assertEquals("example.org", domains.elementAt(1))
        assertEquals("example.com", domains.elementAt(2))
    }
}