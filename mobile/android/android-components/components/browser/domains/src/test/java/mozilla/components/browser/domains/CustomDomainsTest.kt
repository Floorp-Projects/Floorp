/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.domains

import android.content.Context
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class CustomDomainsTest {

    @Before
    fun setUp() {
        testContext.getSharedPreferences("custom_autocomplete", Context.MODE_PRIVATE)
            .edit()
            .clear()
            .apply()
    }

    @ExperimentalCoroutinesApi
    @Test
    fun customListIsEmptyByDefault() {
        val domains = CustomDomains.load(testContext)

        assertEquals(0, domains.size)
    }

    @Test
    fun saveAndRemoveDomains() {
        CustomDomains.save(
            testContext,
            listOf(
                "mozilla.org",
                "example.org",
                "example.com",
            ),
        )

        var domains = CustomDomains.load(testContext)
        assertEquals(3, domains.size)

        CustomDomains.remove(testContext, listOf("example.org", "example.com"))
        domains = CustomDomains.load(testContext)
        assertEquals(1, domains.size)
        assertEquals("mozilla.org", domains.elementAt(0))
    }

    @Test
    fun addAndLoadDomains() {
        CustomDomains.add(testContext, "mozilla.org")
        val domains = CustomDomains.load(testContext)
        assertEquals(1, domains.size)
        assertEquals("mozilla.org", domains.elementAt(0))
    }

    @Test
    fun saveAndLoadDomains() {
        CustomDomains.save(
            testContext,
            listOf(
                "mozilla.org",
                "example.org",
                "example.com",
            ),
        )

        val domains = CustomDomains.load(testContext)

        assertEquals(3, domains.size)
        assertEquals("mozilla.org", domains.elementAt(0))
        assertEquals("example.org", domains.elementAt(1))
        assertEquals("example.com", domains.elementAt(2))
    }
}
