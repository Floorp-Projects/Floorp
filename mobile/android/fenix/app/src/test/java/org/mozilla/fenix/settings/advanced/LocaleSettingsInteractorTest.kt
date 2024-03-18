/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.advanced

import io.mockk.mockk
import io.mockk.verify
import org.junit.Before
import org.junit.Test
import java.util.Locale

class LocaleSettingsInteractorTest {

    private lateinit var interactor: LocaleSettingsInteractor
    private val controller: LocaleSettingsController = mockk(relaxed = true)

    @Before
    fun setup() {
        interactor = LocaleSettingsInteractor(controller)
    }

    @Test
    fun `locale was selected from list`() {
        val locale: Locale = mockk()

        interactor.onLocaleSelected(locale)

        verify { controller.handleLocaleSelected(locale) }
    }

    @Test
    fun `default locale was selected from list`() {
        interactor.onDefaultLocaleSelected()

        verify { controller.handleDefaultLocaleSelected() }
    }

    @Test
    fun `search query was typed`() {
        val query = "Eng"

        interactor.onSearchQueryTyped(query)

        verify { controller.handleSearchQueryTyped(query) }
    }
}
