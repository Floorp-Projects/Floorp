/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.amo.mozilla.components.feature.addons.ui

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.ui.getFormattedAmount
import mozilla.components.feature.addons.ui.translate
import mozilla.components.feature.addons.ui.translatedName
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import java.util.Locale

@RunWith(AndroidJUnit4::class)
class ExtensionsTest {

    @Test
    fun `add-on translateName`() {
        val addon = Addon(
            id = "id",
            authors = emptyList(),
            categories = emptyList(),
            downloadUrl = "downloadUrl",
            version = "version",
            permissions = emptyList(),
            rating = Addon.Rating(4.5f, 1000),
            createdAt = "",
            updatedAt = "",
            translatableName = mapOf("en-US" to "name", "de" to "Name", "es" to "nombre")
        )

        Locale.setDefault(Locale("es"))

        assertEquals("nombre", addon.translatedName)

        Locale.setDefault(Locale.GERMAN)

        assertEquals("Name", addon.translatedName)

        Locale.setDefault(Locale.ENGLISH)

        assertEquals("name", addon.translatedName)
    }

    @Test
    fun translate() {
        val map = mapOf("en-US" to "Hello", "es" to "Hola", "de" to "Hallo")

        Locale.setDefault(Locale("es"))

        assertEquals("Hola", map.translate())

        Locale.setDefault(Locale.GERMAN)

        assertEquals("Hallo", map.translate())

        Locale.setDefault(Locale.ENGLISH)

        assertEquals("Hello", map.translate())
    }

    @Test
    fun getFormattedAmountTest() {
        val amount = 1000

        Locale.setDefault(Locale.ENGLISH)
        assertEquals("1,000", getFormattedAmount(amount))

        Locale.setDefault(Locale.GERMAN)
        assertEquals("1.000", getFormattedAmount(amount))

        Locale.setDefault(Locale("es"))
        assertEquals("1.000", getFormattedAmount(amount))
    }
}
