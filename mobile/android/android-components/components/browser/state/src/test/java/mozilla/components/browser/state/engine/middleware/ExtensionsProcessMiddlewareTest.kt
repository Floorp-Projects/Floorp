/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.engine.middleware

import mozilla.components.browser.state.action.ExtensionsProcessAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.support.test.ext.joinBlocking
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito

class ExtensionsProcessMiddlewareTest {
    private lateinit var engine: Engine
    private lateinit var store: BrowserStore

    @Before
    fun setUp() {
        engine = Mockito.mock()
        store = BrowserStore(
            middleware = listOf(ExtensionsProcessMiddleware(engine)),
            initialState = BrowserState(),
        )
    }

    @Test
    fun `WHEN EnabledAction is dispatched THEN enable the process spawning`() {
        store.dispatch(ExtensionsProcessAction.EnabledAction).joinBlocking()

        Mockito.verify(engine).enableExtensionProcessSpawning()
        Mockito.verify(engine, Mockito.never()).disableExtensionProcessSpawning()
    }

    @Test
    fun `WHEN DisabledAction is dispatched THEN disable the process spawning`() {
        store.dispatch(ExtensionsProcessAction.DisabledAction).joinBlocking()

        Mockito.verify(engine).disableExtensionProcessSpawning()
        Mockito.verify(engine, Mockito.never()).enableExtensionProcessSpawning()
    }
}
