/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.amo.mozilla.components.feature.addons.ui

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.R
import mozilla.components.feature.addons.ui.getFormattedAmount
import mozilla.components.feature.addons.ui.toLocalizedString
import mozilla.components.feature.addons.ui.translate
import mozilla.components.feature.addons.ui.translatedName
import mozilla.components.feature.addons.update.AddonUpdater
import mozilla.components.feature.addons.update.AddonUpdater.Status.NotInstalled
import mozilla.components.feature.addons.update.AddonUpdater.Status.SuccessfullyUpdated
import mozilla.components.feature.addons.update.AddonUpdater.Status.Error
import mozilla.components.feature.addons.update.AddonUpdater.Status.NoUpdateAvailable
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.util.Date
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
            translatableName = mapOf(Addon.DEFAULT_LOCALE to "name", "de" to "Name", "es" to "nombre")
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
        val addon = Addon("id")
        val map = mapOf(addon.defaultLocale to "Hello", "es" to "Hola", "de" to "Hallo")

        Locale.setDefault(Locale("es"))

        assertEquals("Hola", map.translate(addon))

        Locale.setDefault(Locale.GERMAN)

        assertEquals("Hallo", map.translate(addon))

        Locale.setDefault(Locale.ITALIAN)

        assertEquals("Hello", map.translate(addon))
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

    @Test
    fun toLocalizedString() {
        var request = AddonUpdater.UpdateAttempt("addonId", Date(), SuccessfullyUpdated)
        var string = testContext.getString(R.string.mozac_feature_addons_updater_status_successfully_updated)

        assertEquals(string, request.status.toLocalizedString(testContext))

        string = testContext.getString(R.string.mozac_feature_addons_updater_status_no_update_available)
        request = request.copy(status = NoUpdateAvailable)

        assertEquals(string, request.status.toLocalizedString(testContext))

        string = testContext.getString(R.string.mozac_feature_addons_updater_status_error)
        request = request.copy(status = Error("error", Exception()))

        assertTrue(request.status?.toLocalizedString(testContext)!!.contains(string))

        request = request.copy(status = NotInstalled)
        assertEquals("", request.status.toLocalizedString(testContext))
    }
}
