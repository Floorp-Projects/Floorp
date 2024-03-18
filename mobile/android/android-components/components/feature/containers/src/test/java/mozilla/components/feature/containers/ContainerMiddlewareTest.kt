/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.containers

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.flow
import mozilla.components.browser.state.action.ContainerAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContainerState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class ContainerMiddlewareTest {
    @get:Rule
    val mainCoroutineRule = MainCoroutineRule()

    // Test container
    private val container = ContainerState(
        contextId = "contextId",
        name = "Personal",
        color = ContainerState.Color.GREEN,
        icon = ContainerState.Icon.CART,
    )

    @Test
    fun `container storage stores the provided container on add container action`() =
        runTestOnMain {
            val storage = mockStorage()
            val middleware = ContainerMiddleware(testContext, coroutineContext, containerStorage = storage)
            val store = BrowserStore(
                initialState = BrowserState(),
                middleware = listOf(middleware),
            )

            store.waitUntilIdle() // wait to consume InitAction
            store.waitUntilIdle() // wait to consume AddContainersAction

            store.dispatch(ContainerAction.AddContainerAction(container)).joinBlocking()

            verify(storage).addContainer(
                container.contextId,
                container.name,
                container.color,
                container.icon,
            )
        }

    @Test
    fun `fetch the containers from the container storage and load into browser state on initialize container state action`() =
        runTestOnMain {
            val storage = mockStorage(listOf(container))
            val middleware = ContainerMiddleware(testContext, coroutineContext, containerStorage = storage)
            val store = BrowserStore(
                initialState = BrowserState(),
                middleware = listOf(middleware),
            )

            store.waitUntilIdle() // wait to consume InitAction
            store.waitUntilIdle() // wait to consume AddContainersAction

            verify(storage).getContainers()
            assertEquals(container, store.state.containers["contextId"])
        }

    @Test
    fun `container storage removes the provided container on remove container action`() =
        runTestOnMain {
            val storage = mockStorage()
            val middleware = ContainerMiddleware(testContext, coroutineContext, containerStorage = storage)
            val store = BrowserStore(
                initialState = BrowserState(
                    containers = mapOf(
                        container.contextId to container,
                    ),
                ),
                middleware = listOf(middleware),
            )

            store.waitUntilIdle() // wait to consume InitAction
            store.waitUntilIdle() // wait to consume AddContainersAction

            store.dispatch(ContainerAction.RemoveContainerAction(container.contextId))
                .joinBlocking()

            verify(storage).removeContainer(container)
        }

    private fun mockStorage(
        containers: List<ContainerState> = emptyList(),
    ): ContainerStorage {
        val storage: ContainerStorage = mock()
        whenever(storage.getContainers()).thenReturn(
            flow {
                emit(containers)
            },
        )
        return storage
    }
}
