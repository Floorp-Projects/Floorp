/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.helper

import androidx.compose.ui.test.junit4.createComposeRule
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.CustomTabListAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

/**
 * On-device tests for [Target].
 */
@RunWith(AndroidJUnit4::class)
class OnDeviceTargetTest {
    @get:Rule
    val rule = createComposeRule()

    @Test
    fun observingSelectedTab() {
        val store = BrowserStore()

        val target = Target.SelectedTab
        var observedTabId: String? = null

        rule.setContent {
            val state = target.observeAsComposableStateFrom(
                store = store,
                observe = { tab -> tab?.id },
            )
            observedTabId = state.value?.id
        }

        assertNull(observedTabId)

        store.dispatchBlockingOnIdle(
            TabListAction.AddTabAction(createTab("https://www.mozilla.org", id = "mozilla")),
        )

        rule.runOnIdle {
            assertEquals("mozilla", observedTabId)
        }

        store.dispatchBlockingOnIdle(
            TabListAction.AddTabAction(createTab("https://example.org", id = "example")),
        )

        rule.runOnIdle {
            assertEquals("mozilla", observedTabId)
        }

        store.dispatchBlockingOnIdle(
            TabListAction.SelectTabAction("example"),
        )

        rule.runOnIdle {
            assertEquals("example", observedTabId)
        }

        store.dispatchBlockingOnIdle(
            TabListAction.RemoveTabAction("example"),
        )

        rule.runOnIdle {
            assertEquals("mozilla", observedTabId)
        }

        store.dispatchBlockingOnIdle(TabListAction.RemoveAllTabsAction())

        rule.runOnIdle {
            assertNull(observedTabId)
        }
    }

    @Test
    fun observingPinnedTab() {
        val store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "mozilla"),
                    createTab("https://www.example.org", id = "example"),
                ),
                selectedTabId = "mozilla",
            ),
        )

        val target = Target.Tab("mozilla")
        var observedTabId: String? = null

        rule.setContent {
            val state = target.observeAsComposableStateFrom(
                store = store,
                observe = { tab -> tab?.id },
            )
            observedTabId = state.value?.id
        }

        assertEquals("mozilla", observedTabId)

        store.dispatchBlockingOnIdle(TabListAction.SelectTabAction("example"))

        rule.runOnIdle {
            assertEquals("mozilla", observedTabId)
        }

        store.dispatchBlockingOnIdle(TabListAction.RemoveTabAction("mozilla"))

        rule.runOnIdle {
            assertNull(observedTabId)
        }
    }

    @Test
    fun observingCustomTab() {
        val store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "mozilla"),
                    createTab("https://www.example.org", id = "example"),
                ),
                customTabs = listOf(
                    createCustomTab("https://www.reddit.com/r/firefox/", id = "reddit"),
                ),
                selectedTabId = "mozilla",
            ),
        )

        val target = Target.CustomTab("reddit")

        var observedTabId: String? = null

        rule.setContent {
            val state = target.observeAsComposableStateFrom(
                store = store,
                observe = { tab -> tab?.id },
            )
            observedTabId = state.value?.id
        }

        assertEquals("reddit", observedTabId)

        store.dispatchBlockingOnIdle(TabListAction.SelectTabAction("example"))

        rule.runOnIdle {
            assertEquals("reddit", observedTabId)
        }

        store.dispatchBlockingOnIdle(TabListAction.RemoveTabAction("mozilla"))

        rule.runOnIdle {
            assertEquals("reddit", observedTabId)
        }

        store.dispatchBlockingOnIdle(CustomTabListAction.RemoveCustomTabAction("reddit"))

        rule.runOnIdle {
            assertNull(observedTabId)
        }
    }

    private fun BrowserStore.dispatchBlockingOnIdle(action: BrowserAction) {
        rule.runOnIdle {
            val job = dispatch(action)
            runBlocking { job.join() }
        }
    }
}
