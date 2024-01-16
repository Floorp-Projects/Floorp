/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.translations

import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.translate.DetectedLanguages
import mozilla.components.concept.engine.translate.TranslationOptions
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.translations.TranslationsController
import org.mozilla.fenix.translations.TranslationsInteractor

@RunWith(FenixRobolectricTestRunner::class)
class TranslationsInteractorTest {

    val tab: TabSessionState = spy(
        createTab(
            url = "https://www.firefox.com",
            title = "Firefox",
            id = "1",
        ),
    )
    private val tabs = spy(listOf(tab))
    private val browserState = spy(BrowserState(tabs = tabs))
    private val browserStore = spy(BrowserStore(browserState))

    private val translationsUseCase: SessionUseCases.TranslateUseCase = mock()
    private val translationsController = spy(TranslationsController(translationUseCase = translationsUseCase, browserStore = browserStore, tabId = tab.id))
    private val interactor = TranslationsInteractor(translationsController)

    @Test
    fun `Interactor onTranslate called the translate controller as expected`() {
        val from = "en"
        val to = "es"
        val options = TranslationOptions(false)
        interactor.onTranslate(tab.id, from, to, options)
        verify(translationsController).translate(tab.id, from, to, options)
        verify(translationsController, never()).getDetectedLanguages()
    }

    @Test
    fun `Interactor onTranslate called the translate controller as expected when languages are null`() {
        val mockFrom = "es"
        val mockTo = "en"
        val mockDetectedLanguages = DetectedLanguages(
            documentLangTag = mockFrom,
            supportedDocumentLang = true,
            userPreferredLangTag = mockTo,
        )
        whenever(translationsController.getDetectedLanguages()).thenReturn(mockDetectedLanguages)

        val from = null
        val to = null
        val options = TranslationOptions(false)
        interactor.onTranslate(tab.id, from, to, options)
        verify(translationsController).translate(tab.id, from, to, options)
        verify(translationsController).getDetectedLanguages()
        verify(translationsUseCase).invoke(tab.id, mockFrom, mockTo, options)
    }

    @Test
    fun `Interactor detectedLanguages called the controller to pull detected languages`() {
        interactor.detectedLanguages()
        verify(translationsController).getDetectedLanguages()
    }
}
