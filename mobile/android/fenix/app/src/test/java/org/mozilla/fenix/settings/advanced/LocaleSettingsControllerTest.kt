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
import io.mockk.verify
import mozilla.components.support.locale.LocaleManager
import mozilla.components.support.test.mock
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
    fun `set a new locale from the list`() {
        val selectedLocale = Locale("en", "UK")
        val otherLocale: Locale = mock()
        every { localeSettingsStore.state } returns LocaleSettingsState(
            mockk(),
            mockk(),
            otherLocale
        )
        mockkObject(LocaleManager)
        every {
            LocaleManager.setNewLocale(
                context,
                selectedLocale.toLanguageTag()
            )
        } returns context

        controller.handleLocaleSelected(selectedLocale)

        verify { localeSettingsStore.dispatch(LocaleSettingsAction.Select(selectedLocale)) }
        verify { LocaleManager.setNewLocale(context, selectedLocale.toLanguageTag()) }
        verify { (context as Activity).recreate() }
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
        every { LocaleManager.resetToSystemDefault(context) } just Runs

        controller.handleDefaultLocaleSelected()

        verify { localeSettingsStore.dispatch(LocaleSettingsAction.Select(selectedLocale)) }
        verify { LocaleManager.resetToSystemDefault(context) }
        verify { (context as Activity).recreate() }
    }

    @Test
    fun `handle search query typed`() {
        val query = "Eng"

        controller.handleSearchQueryTyped(query)

        verify { localeSettingsStore.dispatch(LocaleSettingsAction.Search(query)) }
    }
}
