/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.action.TranslationsAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.translate.DetectedLanguages
import mozilla.components.concept.engine.translate.Language
import mozilla.components.concept.engine.translate.TranslationDownloadSize
import mozilla.components.concept.engine.translate.TranslationEngineState
import mozilla.components.concept.engine.translate.TranslationError
import mozilla.components.concept.engine.translate.TranslationOperation
import mozilla.components.concept.engine.translate.TranslationPair
import mozilla.components.concept.engine.translate.TranslationSupport
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mozilla.fenix.R

@RunWith(AndroidJUnit4::class)
class TranslationsDialogBindingTest {
    @get:Rule
    val coroutineRule = MainCoroutineRule()

    lateinit var browserStore: BrowserStore
    private lateinit var translationsDialogStore: TranslationsDialogStore

    private val tabId = "1"
    private val tab = createTab(url = tabId, id = tabId)

    @Test
    fun `WHEN fromLanguage and toLanguage get updated in the browserStore THEN translations dialog actions dispatched with the update`() =
        runTestOnMain {
            val englishLanguage = Language("en", "English")
            val spanishLanguage = Language("es", "Spanish")
            translationsDialogStore = spy(TranslationsDialogStore(TranslationsDialogState()))
            browserStore = BrowserStore(
                BrowserState(
                    tabs = listOf(tab),
                    selectedTabId = tabId,
                ),
            )

            val binding = TranslationsDialogBinding(
                browserStore = browserStore,
                translationsDialogStore = translationsDialogStore,
                sessionId = tabId,
                getTranslatedPageTitle = { localizedFrom, localizedTo ->
                    testContext.getString(
                        R.string.translations_bottom_sheet_title_translation_completed,
                        localizedFrom,
                        localizedTo,
                    )
                },
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
                    supportedLanguages = supportLanguages,
                ),
            ).joinBlocking()

            browserStore.dispatch(
                TranslationsAction.TranslateStateChangeAction(
                    tabId = tabId,
                    translationEngineState = translationEngineState,
                ),
            ).joinBlocking()

            verify(translationsDialogStore).dispatch(
                TranslationsDialogAction.UpdateFromSelectedLanguage(
                    englishLanguage,
                ),
            )
            verify(translationsDialogStore).dispatch(
                TranslationsDialogAction.UpdateToSelectedLanguage(
                    spanishLanguage,
                ),
            )
            verify(translationsDialogStore).dispatch(
                TranslationsDialogAction.UpdateTranslatedPageTitle(
                    testContext.getString(
                        R.string.translations_bottom_sheet_title_translation_completed,
                        englishLanguage.localizedDisplayName,
                        spanishLanguage.localizedDisplayName,
                    ),
                ),
            )
        }

    @Test
    fun `WHEN translate action is sent to the browserStore THEN update translation dialog store based on operation`() =
        runTestOnMain {
            val englishLanguage = Language("en", "English")
            val spanishLanguage = Language("es", "Spanish")
            translationsDialogStore = spy(TranslationsDialogStore(TranslationsDialogState()))
            browserStore = BrowserStore(
                BrowserState(
                    tabs = listOf(tab),
                    selectedTabId = tabId,
                ),
            )

            val binding = TranslationsDialogBinding(
                browserStore = browserStore,
                translationsDialogStore = translationsDialogStore,
                sessionId = tabId,
                getTranslatedPageTitle = { localizedFrom, localizedTo ->
                    testContext.getString(
                        R.string.translations_bottom_sheet_title_translation_completed,
                        localizedFrom,
                        localizedTo,
                    )
                },
            )
            binding.start()

            browserStore.dispatch(
                TranslationsAction.TranslateAction(
                    tabId = tabId,
                    fromLanguage = englishLanguage.code,
                    toLanguage = spanishLanguage.code,
                    null,
                ),
            ).joinBlocking()

            verify(translationsDialogStore).dispatch(
                TranslationsDialogAction.UpdateTranslationInProgress(
                    true,
                ),
            )
            verify(translationsDialogStore).dispatch(
                TranslationsDialogAction.DismissDialog(
                    dismissDialogState = DismissDialogState.WaitingToBeDismissed,
                ),
            )
        }

    @Test
    fun `WHEN translate from languages list and translate to languages list are sent to the browserStore THEN update translation dialog store based on operation`() =
        runTestOnMain {
            translationsDialogStore = spy(TranslationsDialogStore(TranslationsDialogState()))
            browserStore = BrowserStore(
                BrowserState(
                    tabs = listOf(tab),
                    selectedTabId = tabId,
                ),
            )

            val binding = TranslationsDialogBinding(
                browserStore = browserStore,
                translationsDialogStore = translationsDialogStore,
                sessionId = tabId,
                getTranslatedPageTitle = { localizedFrom, localizedTo ->
                    testContext.getString(
                        R.string.translations_bottom_sheet_title_translation_completed,
                        localizedFrom,
                        localizedTo,
                    )
                },
            )
            binding.start()

            val toLanguage = Language("de", "German")
            val fromLanguage = Language("es", "Spanish")
            val supportedLanguages = TranslationSupport(listOf(fromLanguage), listOf(toLanguage))
            browserStore.dispatch(
                TranslationsAction.SetSupportedLanguagesAction(
                    supportedLanguages = supportedLanguages,
                ),
            ).joinBlocking()

            verify(translationsDialogStore).dispatch(
                TranslationsDialogAction.UpdateTranslateFromLanguages(
                    listOf(fromLanguage),
                ),
            )
            verify(translationsDialogStore).dispatch(
                TranslationsDialogAction.UpdateTranslateToLanguages(
                    listOf(toLanguage),
                ),
            )
        }

    @Test
    fun `WHEN translate action success is sent to the browserStore THEN update translation dialog store based on operation`() =
        runTestOnMain {
            translationsDialogStore =
                spy(TranslationsDialogStore(TranslationsDialogState(dismissDialogState = DismissDialogState.WaitingToBeDismissed)))
            browserStore = BrowserStore(
                BrowserState(
                    tabs = listOf(tab),
                    selectedTabId = tabId,
                ),
            )

            val binding = TranslationsDialogBinding(
                browserStore = browserStore,
                translationsDialogStore = translationsDialogStore,
                sessionId = tabId,
                getTranslatedPageTitle = { localizedFrom, localizedTo ->
                    testContext.getString(
                        R.string.translations_bottom_sheet_title_translation_completed,
                        localizedFrom,
                        localizedTo,
                    )
                },
            )
            binding.start()

            browserStore.dispatch(
                TranslationsAction.TranslateSuccessAction(
                    tabId = tab.id,
                    operation = TranslationOperation.TRANSLATE,
                ),
            ).joinBlocking()

            verify(translationsDialogStore).dispatch(
                TranslationsDialogAction.UpdateTranslated(
                    true,
                ),
            )
            verify(translationsDialogStore).dispatch(
                TranslationsDialogAction.UpdateTranslationInProgress(
                    false,
                ),
            )
            verify(translationsDialogStore).dispatch(
                TranslationsDialogAction.DismissDialog(
                    dismissDialogState = DismissDialogState.Dismiss,
                ),
            )
        }

    @Test
    fun `WHEN translate fetch error is sent to the browserStore THEN update translation dialog store based on operation`() =
        runTestOnMain {
            translationsDialogStore =
                spy(TranslationsDialogStore(TranslationsDialogState()))
            browserStore = BrowserStore(
                BrowserState(
                    tabs = listOf(tab),
                    selectedTabId = tabId,
                ),
            )

            val binding = TranslationsDialogBinding(
                browserStore = browserStore,
                translationsDialogStore = translationsDialogStore,
                sessionId = tabId,
                getTranslatedPageTitle = { localizedFrom, localizedTo ->
                    testContext.getString(
                        R.string.translations_bottom_sheet_title_translation_completed,
                        localizedFrom,
                        localizedTo,
                    )
                },
            )
            binding.start()

            val fetchError = TranslationError.CouldNotLoadLanguagesError(null)
            browserStore.dispatch(
                TranslationsAction.TranslateExceptionAction(
                    tabId = tab.id,
                    operation = TranslationOperation.FETCH_SUPPORTED_LANGUAGES,
                    translationError = fetchError,
                ),
            ).joinBlocking()

            verify(translationsDialogStore).dispatch(
                TranslationsDialogAction.UpdateTranslationError(fetchError),
            )
        }

    @Test
    fun `WHEN set translation download size action sent to the browserStore THEN update translation dialog store based on operation`() =
        runTestOnMain {
            translationsDialogStore =
                spy(TranslationsDialogStore(TranslationsDialogState()))
            browserStore = BrowserStore(
                BrowserState(
                    tabs = listOf(tab),
                    selectedTabId = tabId,
                ),
            )

            val binding = TranslationsDialogBinding(
                browserStore = browserStore,
                translationsDialogStore = translationsDialogStore,
                sessionId = tabId,
                getTranslatedPageTitle = { localizedFrom, localizedTo ->
                    testContext.getString(
                        R.string.translations_bottom_sheet_title_translation_completed,
                        localizedFrom,
                        localizedTo,
                    )
                },
            )
            binding.start()

            val toLanguage = Language("de", "German")
            val fromLanguage = Language("es", "Spanish")
            val translationDownloadSize = TranslationDownloadSize(
                fromLanguage = fromLanguage,
                toLanguage = toLanguage,
                size = 1000L,
            )
            browserStore.dispatch(
                TranslationsAction.SetTranslationDownloadSizeAction(
                    tabId = tab.id,
                    translationSize = translationDownloadSize,
                ),
            ).joinBlocking()

            verify(translationsDialogStore).dispatch(
                TranslationsDialogAction.UpdateDownloadTranslationDownloadSize(translationDownloadSize),
            )
        }
}
