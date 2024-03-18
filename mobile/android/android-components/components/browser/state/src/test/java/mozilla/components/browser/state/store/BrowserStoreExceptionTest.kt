/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.store

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.action.TabGroupAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabGroup
import mozilla.components.browser.state.state.TabPartition
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.state.recover.toRecoverableTab
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

            store.dispatch(TabListAction.RestoreAction(listOf(tab1.toRecoverableTab()), restoreLocation = TabListAction.RestoreAction.RestoreLocation.BEGINNING)).joinBlocking()
        }
    }

    @Test(expected = IllegalArgumentException::class)
    fun `AddMultipleTabsAction - Exception is thrown in tab with id already exists`() {
        unwrapStoreExceptionAndRethrow {
            val store = BrowserStore(
                BrowserState(
                    tabs = listOf(
                        createTab(id = "a", url = "https://www.mozilla.org"),
                    ),
                ),
            )

            store.dispatch(
                TabListAction.AddMultipleTabsAction(
                    tabs = listOf(
                        createTab(id = "a", url = "https://www.example.org"),
                    ),
                ),
            ).joinBlocking()
        }
    }

    @Test(expected = IllegalArgumentException::class)
    fun `AddMultipleTabsAction - Exception is thrown if parent id is set`() {
        unwrapStoreExceptionAndRethrow {
            val store = BrowserStore()

            val tab1 = createTab(
                id = "a",
                url = "https://www.mozilla.org",
            )

            val tab2 = createTab(
                id = "b",
                url = "https://www.firefox.com",
                private = true,
                parent = tab1,
            )

            store.dispatch(
                TabListAction.AddMultipleTabsAction(
                    tabs = listOf(tab1, tab2),
                ),
            ).joinBlocking()
        }
    }

    @Test(expected = java.lang.IllegalArgumentException::class)
    fun `AddTabGroupAction - Exception is thrown when group already exists`() {
        unwrapStoreExceptionAndRethrow {
            val partitionId = "testFeaturePartition"
            val testGroup = TabGroup("test")
            val store = BrowserStore(
                BrowserState(
                    tabPartitions = mapOf(partitionId to TabPartition(partitionId, tabGroups = listOf(testGroup))),
                ),
            )

            store.dispatch(
                TabGroupAction.AddTabGroupAction(
                    partition = partitionId,
                    group = testGroup,
                ),
            ).joinBlocking()
        }
    }

    @Test(expected = java.lang.IllegalArgumentException::class)
    fun `AddTabGroupAction - Asserts that tabs exist`() {
        unwrapStoreExceptionAndRethrow {
            val store = BrowserStore()

            val partition = "testFeaturePartition"
            val testGroup = TabGroup("test", tabIds = listOf("invalid"))
            store.dispatch(
                TabGroupAction.AddTabGroupAction(
                    partition = partition,
                    group = testGroup,
                ),
            ).joinBlocking()
        }
    }

    @Test(expected = java.lang.IllegalArgumentException::class)
    fun `AddTabAction - Asserts that tab exists when adding to group`() {
        unwrapStoreExceptionAndRethrow {
            val tabGroup = TabGroup("test1", tabIds = emptyList())
            val tabPartition = TabPartition("testFeaturePartition", tabGroups = listOf(tabGroup))

            val store = BrowserStore(
                BrowserState(
                    tabs = listOf(),
                    tabPartitions = mapOf("testFeaturePartition" to tabPartition),
                ),
            )

            val tab = createTab(id = "tab1", url = "https://firefox.com")
            store.dispatch(TabGroupAction.AddTabAction(tabPartition.id, tabGroup.id, tab.id)).joinBlocking()
        }
    }

    @Test(expected = java.lang.IllegalArgumentException::class)
    fun `AddTabsAction - Asserts that tabs exist when adding to group`() {
        unwrapStoreExceptionAndRethrow {
            val tabGroup = TabGroup("test1", tabIds = emptyList())
            val tabPartition = TabPartition("testFeaturePartition", tabGroups = listOf(tabGroup))
            val tab1 = createTab(id = "tab1", url = "https://firefox.com")
            val tab2 = createTab(id = "tab2", url = "https://mozilla.org")

            val store = BrowserStore(
                BrowserState(
                    tabs = listOf(tab1),
                    tabPartitions = mapOf("testFeaturePartition" to tabPartition),
                ),
            )

            store.dispatch(
                TabGroupAction.AddTabsAction(tabPartition.id, tabGroup.id, listOf(tab1.id, tab2.id)),
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
