/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.action

import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.translate.TranslationOperation
import mozilla.components.support.test.ext.joinBlocking
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import java.lang.Exception

class TranslationsActionTest {
    private lateinit var tab: TabSessionState
    private lateinit var store: BrowserStore

    @Before
    fun setUp() {
        tab = createTab("https://www.mozilla.org")

        store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(tab),
            ),
        )
    }

    private fun tabState(): TabSessionState = store.state.findTab(tab.id)!!

    @Test
    fun `WHEN a TranslateAction is dispatched AND successful THEN update translation processing status`() {
        // Initial
        assertEquals(false, tabState().translationsState.isTranslateProcessing)

        // Action started
        store.dispatch(TranslationsAction.TranslateAction(tabId = tab.id, "en", "es", null))
            .joinBlocking()
        assertEquals(true, tabState().translationsState.isTranslateProcessing)

        // Action success
        store.dispatch(TranslationsAction.TranslateSuccessAction(tabId = tab.id, operation = TranslationOperation.TRANSLATE))
            .joinBlocking()
        assertEquals(false, tabState().translationsState.isTranslateProcessing)
        assertEquals(true, tabState().translationsState.isTranslated)
    }

    @Test
    fun `WHEN a TranslateAction is dispatched AND fails THEN update translation processing status`() {
        // Initial
        assertEquals(false, tabState().translationsState.isTranslateProcessing)

        // Action started
        store.dispatch(TranslationsAction.TranslateAction(tabId = tab.id, "en", "es", null))
            .joinBlocking()
        assertEquals(true, tabState().translationsState.isTranslateProcessing)

        // Action failure
        store.dispatch(TranslationsAction.TranslateExceptionAction(tabId = tab.id, operation = TranslationOperation.TRANSLATE, Exception()))
            .joinBlocking()
        assertEquals(false, tabState().translationsState.isTranslateProcessing)
        assertEquals(false, tabState().translationsState.isTranslated)
    }

    @Test
    fun `WHEN a TranslateRestoreAction is dispatched AND successful THEN update translation processing status`() {
        // Initial
        assertEquals(false, tabState().translationsState.isRestoreProcessing)

        // Action started
        store.dispatch(TranslationsAction.TranslateRestoreAction(tabId = tab.id))
            .joinBlocking()
        assertEquals(true, tabState().translationsState.isRestoreProcessing)

        // Action success
        store.dispatch(TranslationsAction.TranslateSuccessAction(tabId = tab.id, operation = TranslationOperation.RESTORE))
            .joinBlocking()
        assertEquals(false, tabState().translationsState.isRestoreProcessing)
        assertEquals(false, tabState().translationsState.isTranslated)
    }

    @Test
    fun `WHEN a TranslateRestoreAction is dispatched AND fails THEN update translation processing status`() {
        // Initial
        assertEquals(false, tabState().translationsState.isRestoreProcessing)

        // Action started
        store.dispatch(TranslationsAction.TranslateRestoreAction(tabId = tab.id))
            .joinBlocking()
        assertEquals(true, tabState().translationsState.isRestoreProcessing)

        // Action failure
        store.dispatch(TranslationsAction.TranslateExceptionAction(tabId = tab.id, operation = TranslationOperation.RESTORE, Exception()))
            .joinBlocking()
        assertEquals(false, tabState().translationsState.isRestoreProcessing)
        assertEquals(false, tabState().translationsState.isTranslated)
    }
}
