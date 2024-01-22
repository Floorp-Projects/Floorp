/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations

import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.test.runTest
import mozilla.components.browser.state.action.TranslationsAction
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.translate.Language
import mozilla.components.concept.engine.translate.TranslationOperation
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class TranslationsDialogMiddlewareTest {

    @Test
    fun `GIVEN translationState WHEN FetchSupportedLanguages action is called THEN call OperationRequestedAction from BrowserStore`() =
        runTest {
            val browserStore = mockk<BrowserStore>(relaxed = true)
            val translationsDialogMiddleware =
                TranslationsDialogMiddleware(browserStore = browserStore, sessionId = "tab1")

            val translationStore = TranslationsDialogStore(
                initialState = TranslationsDialogState(),
                middlewares = listOf(translationsDialogMiddleware),
            )
            translationStore.dispatch(TranslationsDialogAction.FetchSupportedLanguages).joinBlocking()

            translationStore.waitUntilIdle()

            verify {
                browserStore.dispatch(
                    TranslationsAction.OperationRequestedAction(
                        tabId = "tab1",
                        operation = TranslationOperation.FETCH_SUPPORTED_LANGUAGES,
                    ),
                )
            }
        }

    @Test
    fun `GIVEN translationState WHEN TranslateAction from TranslationDialogStore is called THEN call TranslateAction from BrowserStore`() =
        runTest {
            val browserStore = mockk<BrowserStore>(relaxed = true)
            val translationsDialogMiddleware =
                TranslationsDialogMiddleware(browserStore = browserStore, sessionId = "tab1")

            val translationStore = TranslationsDialogStore(
                initialState = TranslationsDialogState(
                    initialFrom = Language("en", "English"),
                    initialTo = Language("fr", "France"),
                ),
                middlewares = listOf(translationsDialogMiddleware),
            )
            translationStore.dispatch(TranslationsDialogAction.TranslateAction).joinBlocking()

            translationStore.waitUntilIdle()

            verify {
                browserStore.dispatch(
                    TranslationsAction.TranslateAction(
                        tabId = "tab1",
                        fromLanguage = "en",
                        toLanguage = "fr",
                        options = null,
                    ),
                )
            }
        }

    @Test
    fun `GIVEN translationState WHEN RestoreTranslation from TranslationDialogStore is called THEN call TranslateRestoreAction from BrowserStore`() =
        runTest {
            val browserStore = mockk<BrowserStore>(relaxed = true)
            val translationsDialogMiddleware =
                TranslationsDialogMiddleware(browserStore = browserStore, sessionId = "tab1")

            val translationStore = TranslationsDialogStore(
                initialState = TranslationsDialogState(),
                middlewares = listOf(translationsDialogMiddleware),
            )
            translationStore.dispatch(TranslationsDialogAction.RestoreTranslation).joinBlocking()

            translationStore.waitUntilIdle()

            verify {
                browserStore.dispatch(
                    TranslationsAction.TranslateRestoreAction(
                        tabId = "tab1",
                    ),
                )
            }
        }
}
