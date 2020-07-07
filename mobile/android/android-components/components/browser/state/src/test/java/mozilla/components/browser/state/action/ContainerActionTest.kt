/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.action

import mozilla.components.browser.state.state.ContainerState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.ext.joinBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test

class ContainerActionTest {

    @Test
    fun `AddContainerAction - Adds a container to the BrowserState containers`() {
        val store = BrowserStore()

        assertTrue(store.state.containers.isEmpty())

        val container = ContainerState(
            contextId = "contextId",
            name = "Personal",
            color = ContainerState.Color.GREEN,
            icon = ContainerState.Icon.CART
        )
        store.dispatch(ContainerAction.AddContainerAction(container)).joinBlocking()

        assertFalse(store.state.containers.isEmpty())
        assertEquals(container, store.state.containers.values.first())

        val state = store.state
        store.dispatch(ContainerAction.AddContainerAction(container)).joinBlocking()
        assertSame(state, store.state)
    }

    @Test
    fun `RemoveContainerAction - Removes a container from the BrowserState containers`() {
        val store = BrowserStore()

        assertTrue(store.state.containers.isEmpty())

        val container1 = ContainerState(
            contextId = "1",
            name = "Personal",
            color = ContainerState.Color.BLUE,
            icon = ContainerState.Icon.BRIEFCASE
        )
        val container2 = ContainerState(
            contextId = "2",
            name = "Shopping",
            color = ContainerState.Color.GREEN,
            icon = ContainerState.Icon.CIRCLE
        )
        store.dispatch(ContainerAction.AddContainerAction(container1)).joinBlocking()
        store.dispatch(ContainerAction.AddContainerAction(container2)).joinBlocking()

        assertFalse(store.state.containers.isEmpty())
        assertEquals(container1, store.state.containers.values.first())
        assertEquals(container2, store.state.containers.values.last())

        store.dispatch(ContainerAction.RemoveContainerAction(container1.contextId)).joinBlocking()

        assertEquals(1, store.state.containers.size)
        assertEquals(container2, store.state.containers.values.first())
    }
}
