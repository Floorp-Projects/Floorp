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
            icon = ContainerState.Icon.CART,
        )
        store.dispatch(ContainerAction.AddContainerAction(container)).joinBlocking()

        assertFalse(store.state.containers.isEmpty())
        assertEquals(container, store.state.containers.values.first())

        val state = store.state
        store.dispatch(ContainerAction.AddContainerAction(container)).joinBlocking()
        assertSame(state, store.state)
    }

    @Test
    fun `AddContainersAction - Adds a list of containers to the BrowserState containers`() {
        val store = BrowserStore()

        assertTrue(store.state.containers.isEmpty())

        val container1 = ContainerState(
            contextId = "1",
            name = "Personal",
            color = ContainerState.Color.GREEN,
            icon = ContainerState.Icon.CART,
        )
        val container2 = ContainerState(
            contextId = "2",
            name = "Work",
            color = ContainerState.Color.RED,
            icon = ContainerState.Icon.FINGERPRINT,
        )
        val container3 = ContainerState(
            contextId = "3",
            name = "Shopping",
            color = ContainerState.Color.BLUE,
            icon = ContainerState.Icon.BRIEFCASE,
        )
        store.dispatch(ContainerAction.AddContainersAction(listOf(container1, container2))).joinBlocking()

        assertFalse(store.state.containers.isEmpty())
        assertEquals(container1, store.state.containers.values.first())
        assertEquals(container2, store.state.containers.values.last())

        // Assert that the state remains the same if the existing containers are re-added.
        val state = store.state
        store.dispatch(ContainerAction.AddContainersAction(listOf(container1, container2))).joinBlocking()
        assertSame(state, store.state)

        // Assert that only non-existing containers are added.
        store.dispatch(ContainerAction.AddContainersAction(listOf(container1, container2, container3))).joinBlocking()
        assertEquals(3, store.state.containers.size)
        assertEquals(container1, store.state.containers.values.first())
        assertEquals(container2, store.state.containers.values.elementAt(1))
        assertEquals(container3, store.state.containers.values.last())
    }

    @Test
    fun `RemoveContainerAction - Removes a container from the BrowserState containers`() {
        val store = BrowserStore()

        assertTrue(store.state.containers.isEmpty())

        val container1 = ContainerState(
            contextId = "1",
            name = "Personal",
            color = ContainerState.Color.BLUE,
            icon = ContainerState.Icon.BRIEFCASE,
        )
        val container2 = ContainerState(
            contextId = "2",
            name = "Shopping",
            color = ContainerState.Color.GREEN,
            icon = ContainerState.Icon.CIRCLE,
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
