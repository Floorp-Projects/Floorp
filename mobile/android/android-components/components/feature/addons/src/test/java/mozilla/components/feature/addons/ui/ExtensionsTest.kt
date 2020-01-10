/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.amo.mozilla.components.feature.addons.ui

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.feature.addons.ui.translate
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import java.util.Locale

@RunWith(AndroidJUnit4::class)
class ExtensionsTest {

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
}
