/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.toolbar

import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContainerState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

class ContainerToolbarFeatureTest {
    // Test container
    private val container = ContainerState(
        contextId = "1",
        name = "Personal",
        color = ContainerState.Color.GREEN,
        icon = ContainerState.Icon.FINGERPRINT,
    )

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `render a container action from browser state`() {
        val toolbar: Toolbar = mock()
        val store = spy(
            BrowserStore(
                BrowserState(
                    tabs = listOf(
                        createTab("https://www.example.org", id = "tab1", contextId = "1"),
                    ),
                    selectedTabId = "tab1",
                    containers = mapOf(
                        container.contextId to container,
                    ),
                ),
            ),
        )
        val containerToolbarFeature = getContainerToolbarFeature(toolbar, store)

        verify(store).observeManually(any())
        verify(containerToolbarFeature).renderContainerAction(any(), any())

        val pageActionCaptor = argumentCaptor<ContainerToolbarAction>()
        verify(toolbar).addPageAction(pageActionCaptor.capture())
        assertEquals(container, pageActionCaptor.value.container)
    }

    @Test
    fun `remove container page action when selecting a normal tab`() {
        val toolbar: Toolbar = mock()
        val store = spy(
            BrowserStore(
                BrowserState(
                    tabs = listOf(
                        createTab("https://www.example.org", id = "tab1", contextId = "1"),
                        createTab("https://www.mozilla.org", id = "tab2"),
                    ),
                    selectedTabId = "tab1",
                    containers = mapOf(
                        container.contextId to container,
                    ),
                ),
            ),
        )
        val containerToolbarFeature = getContainerToolbarFeature(toolbar, store)
        store.dispatch(TabListAction.SelectTabAction("tab2")).joinBlocking()
        coroutinesTestRule.testDispatcher.scheduler.advanceUntilIdle()

        verify(store).observeManually(any())
        verify(containerToolbarFeature, times(2)).renderContainerAction(any(), any())
        verify(toolbar).removePageAction(any())
    }

    private fun getContainerToolbarFeature(
        toolbar: Toolbar = mock(),
        store: BrowserStore = BrowserStore(),
    ): ContainerToolbarFeature {
        val containerToolbarFeature = spy(ContainerToolbarFeature(toolbar, store))
        containerToolbarFeature.start()
        return containerToolbarFeature
    }
}
