/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.store

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.lib.state.StoreException
import mozilla.components.support.test.ext.joinBlocking
import org.junit.Assert.fail
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.shadows.ShadowLooper
import java.lang.IllegalArgumentException

// These tests are in a separate class because they needs to run with
// Robolectric (different runner, slower) while all other tests only
// need a Java VM (fast).
@RunWith(AndroidJUnit4::class)
class BrowserStoreExceptionTest {

    @Test(expected = java.lang.IllegalArgumentException::class)
    fun `AddTabAction - Exception is thrown if parent doesn't exist`() {
        unwrapStoreExceptionAndRethrow {
            val store = BrowserStore()
            val parent = createTab("https://www.mozilla.org")
            val child = createTab("https://www.firefox.com", parent = parent)

            store.dispatch(TabListAction.AddTabAction(child)).joinBlocking()
        }
    }

    @Test(expected = java.lang.IllegalArgumentException::class)
    fun `AddTabAction - Exception is thrown if tab already exists`() {
        unwrapStoreExceptionAndRethrow {
            val store = BrowserStore()
            val tab1 = createTab("https://www.mozilla.org")
            store.dispatch(TabListAction.AddTabAction(tab1)).joinBlocking()
            store.dispatch(TabListAction.AddTabAction(tab1)).joinBlocking()
        }
    }

    @Test(expected = java.lang.IllegalArgumentException::class)
    fun `RestoreTabAction - Exception is thrown if tab already exists`() {
        unwrapStoreExceptionAndRethrow {
            val store = BrowserStore()
            val tab1 = createTab("https://www.mozilla.org")
            store.dispatch(TabListAction.AddTabAction(tab1)).joinBlocking()

            store.dispatch(TabListAction.RestoreAction(listOf(tab1))).joinBlocking()
        }
    }

    @Test(expected = IllegalArgumentException::class)
    fun `AddMultipleTabsAction - Exception is thrown in tab with id already exists`() {
        unwrapStoreExceptionAndRethrow {
            val store = BrowserStore(BrowserState(
                    tabs = listOf(
                            createTab(id = "a", url = "https://www.mozilla.org")
                    )
            ))

            store.dispatch(TabListAction.AddMultipleTabsAction(
                    tabs = listOf(
                            createTab(id = "a", url = "https://www.example.org")
                    )
            )).joinBlocking()
        }
    }

    @Test(expected = IllegalArgumentException::class)
    fun `AddMultipleTabsAction - Exception is thrown if parent id is set`() {
        unwrapStoreExceptionAndRethrow {
            val store = BrowserStore()

            val tab1 = createTab(
                    id = "a",
                    url = "https://www.mozilla.org")

            val tab2 = createTab(
                    id = "b",
                    url = "https://www.firefox.com",
                    private = true,
                    parent = tab1)

            store.dispatch(TabListAction.AddMultipleTabsAction(
                    tabs = listOf(tab1, tab2))
            ).joinBlocking()
        }
    }

    private fun unwrapStoreExceptionAndRethrow(block: () -> Unit) {
        try {
            block()

            // Wait for the main looper to process the re-thrown exception.
            ShadowLooper.idleMainLooper()

            fail("Did not throw StoreException")
        } catch (e: StoreException) {
            val cause = e.cause
            if (cause != null) {
                throw cause
            }
        } catch (e: Throwable) {
            fail("Did throw a different exception $e")
        }

        fail("Did not throw StoreException with wrapped exception")
    }
}
