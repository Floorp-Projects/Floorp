/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.locale

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import java.util.Locale

@RunWith(AndroidJUnit4::class)
class LocaleManagerTest {

    @Before
    fun setup() {
        LocaleManager.clear(testContext)
        Locale.setDefault("en_US".toLocale())
    }

    @Test
    fun `changing the language to Spanish must change the system locale to Spanish and change the configurations`() {

        var currentLocale = LocaleManager.getCurrentLocale(testContext)

        assertNull(currentLocale)

        val newContext = LocaleManager.setNewLocale(testContext, "es")

        assertNotEquals(testContext, newContext)

        currentLocale = LocaleManager.getCurrentLocale(testContext)

        assertEquals(currentLocale, "es".toLocale())
        assertEquals(currentLocale, Locale.getDefault())
    }

    @Test
    fun `when calling updateResources with none current language must not change the system locale neither change configurations`() {
        val previousSystemLocale = Locale.getDefault()
        val context = LocaleManager.updateResources(testContext)

        assertEquals(testContext, context)
        assertEquals(previousSystemLocale, Locale.getDefault())
    }
}