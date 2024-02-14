/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.action.TranslationsAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.translate.DetectedLanguages
import mozilla.components.concept.engine.translate.Language
import mozilla.components.concept.engine.translate.TranslationEngineState
import mozilla.components.concept.engine.translate.TranslationOperation
import mozilla.components.concept.engine.translate.TranslationPair
import mozilla.components.concept.engine.translate.TranslationSupport
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class TranslationsBindingTest {
    @get:Rule
    val coroutineRule = MainCoroutineRule()

    lateinit var browserStore: BrowserStore

    private val tabId = "1"
    private val tab = createTab(url = tabId, id = tabId)
    private val onIconChanged: (
        isVisible: Boolean,
        isTranslated: Boolean,
        fromSelectedLanguage: Language?,
        toSelectedLanguage: Language?,
    ) -> Unit = spy()

    @Test
    fun `GIVEN translationState WHEN translation status isTranslated THEN invoke onIconChanged callback`() =
        runTestOnMain {
            val englishLanguage = Language("en", "English")
            val spanishLanguage = Language("es", "Spanish")

            browserStore = BrowserStore(
                BrowserState(
                    tabs = listOf(tab),
                    selectedTabId = tabId,
                ),
            )

            val binding = TranslationsBinding(
                browserStore = browserStore,
                sessionId = tabId,
                onStateUpdated = onIconChanged,
            )
            binding.start()

            val detectedLanguages = DetectedLanguages(
                documentLangTag = englishLanguage.code,
                supportedDocumentLang = true,
                userPreferredLangTag = spanishLanguage.code,
            )

            val translationEngineState = TranslationEngineState(
                detectedLanguages = detectedLanguages,
                error = null,
                isEngineReady = true,
                requestedTranslationPair = TranslationPair(
                    fromLanguage = englishLanguage.code,
                    toLanguage = spanishLanguage.code,
                ),
            )

            val supportLanguages = TranslationSupport(
                fromLanguages = listOf(englishLanguage),
                toLanguages = listOf(spanishLanguage),
            )

            browserStore.dispatch(
                TranslationsAction.SetSupportedLanguagesAction(
                    tabId = tab.id,
                    supportedLanguages = supportLanguages,
                ),
            ).joinBlocking()

            browserStore.dispatch(
                TranslationsAction.TranslateStateChangeAction(
                    tabId = tabId,
                    translationEngineState = translationEngineState,
                ),
            ).joinBlocking()

            browserStore.dispatch(
                TranslationsAction.TranslateSuccessAction(
                    tabId = tab.id,
                    operation = TranslationOperation.TRANSLATE,
                ),
            ).joinBlocking()

            verify(onIconChanged).invoke(
                true,
                true,
                englishLanguage,
                spanishLanguage,
            )
        }

    @Test
    fun `GIVEN translationState WHEN translation status isExpectedTranslate THEN invoke onIconChanged callback`() =
        runTestOnMain {
            browserStore = BrowserStore(
                BrowserState(
                    tabs = listOf(tab),
                    selectedTabId = tabId,
                ),
            )

            val binding = TranslationsBinding(
                browserStore = browserStore,
                sessionId = tabId,
                onStateUpdated = onIconChanged,
            )
            binding.start()

            browserStore.dispatch(
                TranslationsAction.TranslateExpectedAction(
                    tabId = tabId,
                ),
            ).joinBlocking()

            verify(onIconChanged).invoke(
                true,
                false,
                null,
                null,
            )
        }

    @Test
    fun `GIVEN translationState WHEN translation status is not isExpectedTranslate or isTranslated THEN invoke onIconChanged callback`() =
        runTestOnMain {
            browserStore = BrowserStore(
                BrowserState(
                    tabs = listOf(tab),
                    selectedTabId = tabId,
                ),
            )

            val binding = TranslationsBinding(
                browserStore = browserStore,
                sessionId = tabId,
                onStateUpdated = onIconChanged,
            )
            binding.start()

            verify(onIconChanged).invoke(
                false,
                false,
                null,
                null,
            )
        }
}
