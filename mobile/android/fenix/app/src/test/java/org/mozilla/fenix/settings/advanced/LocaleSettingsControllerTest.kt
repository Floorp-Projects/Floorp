/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.advanced

import android.app.Activity
import android.content.Context
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.mockkObject
import io.mockk.mockkStatic
import io.mockk.verify
import mozilla.components.support.locale.LocaleManager
import org.junit.Before
import org.junit.Test
import java.util.Locale

class LocaleSettingsControllerTest {

    private val context: Context = mockk<Activity>(relaxed = true)
    private val localeSettingsStore: LocaleSettingsStore = mockk(relaxed = true)

    private lateinit var controller: LocaleSettingsController

    @Before
    fun setup() {
        controller = DefaultLocaleSettingsController(context, localeSettingsStore)
    }

    @Test
    fun `don't set locale if same locale is chosen`() {
        val selectedLocale = Locale("en", "UK")
        every { localeSettingsStore.state } returns LocaleSettingsState(
            mockk(),
            mockk(),
            selectedLocale
        )
        mockkObject(LocaleManager)
        mockkStatic("org.mozilla.fenix.settings.advanced.LocaleManagerExtensionKt")
        every { LocaleManager.getCurrentLocale(context) } returns mockk()
        every { LocaleManager.isDefaultLocaleSelected(context) } returns false

        controller.handleLocaleSelected(selectedLocale)

        verify(
            inverse = true,
            verifyBlock = { localeSettingsStore.dispatch(LocaleSettingsAction.Select(selectedLocale)) })
        verify(
            inverse = true,
            verifyBlock = { LocaleManager.setNewLocale(context, selectedLocale.toLanguageTag()) })
        verify(
            inverse = true,
            verifyBlock = { LocaleManager.updateBaseConfiguration(context, selectedLocale) })
        verify(inverse = true, verifyBlock = { (context as Activity).recreate() })
    }

    @Test
    fun `set a new locale from the list`() {
        val selectedLocale = Locale("en", "UK")
        val otherLocale: Locale = mockk()
        every { localeSettingsStore.state } returns LocaleSettingsState(
            mockk(),
            mockk(),
            otherLocale
        )
        mockkObject(LocaleManager)
        mockkStatic("org.mozilla.fenix.settings.advanced.LocaleManagerExtensionKt")
        every { LocaleManager.updateBaseConfiguration(context, selectedLocale) } just Runs
        every {
            LocaleManager.setNewLocale(
                context,
                selectedLocale.toLanguageTag()
            )
        } returns context

        controller.handleLocaleSelected(selectedLocale)

        verify { localeSettingsStore.dispatch(LocaleSettingsAction.Select(selectedLocale)) }
        verify { LocaleManager.setNewLocale(context, selectedLocale.toLanguageTag()) }
        verify { LocaleManager.updateBaseConfiguration(context, selectedLocale) }
        verify { (context as Activity).recreate() }
    }

    @Test
    fun `don't set default locale if default locale is already chosen`() {
        val selectedLocale = Locale("en", "UK")
        val localeList = ArrayList<Locale>()
        localeList.add(selectedLocale)
        every { localeSettingsStore.state } returns LocaleSettingsState(
            localeList,
            mockk(),
            mockk()
        )
        mockkStatic("org.mozilla.fenix.settings.advanced.LocaleManagerExtensionKt")
        every { LocaleManager.isDefaultLocaleSelected(context) } returns true

        controller.handleDefaultLocaleSelected()

        verify(
            inverse = true,
            verifyBlock = { localeSettingsStore.dispatch(LocaleSettingsAction.Select(selectedLocale)) })
        verify(
            inverse = true,
            verifyBlock = { LocaleManager.resetToSystemDefault(context) })
        verify(
            inverse = true,
            verifyBlock = { LocaleManager.updateBaseConfiguration(context, selectedLocale) })
        verify(inverse = true, verifyBlock = { (context as Activity).recreate() })
    }

    @Test
    fun `set the default locale as the new locale`() {
        val selectedLocale = Locale("en", "UK")
        val localeList = ArrayList<Locale>()
        localeList.add(selectedLocale)
        every { localeSettingsStore.state } returns LocaleSettingsState(
            localeList,
            mockk(),
            mockk()
        )
        mockkObject(LocaleManager)
        mockkStatic("org.mozilla.fenix.settings.advanced.LocaleManagerExtensionKt")
        every { LocaleManager.resetToSystemDefault(context) } just Runs
        every { LocaleManager.updateBaseConfiguration(context, selectedLocale) } just Runs

        controller.handleDefaultLocaleSelected()

        verify { localeSettingsStore.dispatch(LocaleSettingsAction.Select(selectedLocale)) }
        verify { LocaleManager.resetToSystemDefault(context) }
        verify { LocaleManager.updateBaseConfiguration(context, selectedLocale) }
        verify { (context as Activity).recreate() }
    }

    @Test
    fun `handle search query typed`() {
        val query = "Eng"

        controller.handleSearchQueryTyped(query)

        verify { localeSettingsStore.dispatch(LocaleSettingsAction.Search(query)) }
    }
}
