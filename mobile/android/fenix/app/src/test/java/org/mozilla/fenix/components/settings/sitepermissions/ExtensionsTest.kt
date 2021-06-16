/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.settings.sitepermissions

import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.ktx.kotlin.getOrigin
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.settings.sitepermissions.tryReloadTabBy

@RunWith(FenixRobolectricTestRunner::class)
class ExtensionsTest {

    @Test
    fun `tryReloadTabBy reloads latest tab matching origin`() {
        val store = BrowserStore(
            BrowserState(tabs = listOf(
                    createTab(id = "1", url = "https://www.mozilla.org/1", lastAccess = 1),
                    createTab(id = "2", url = "https://www.mozilla.org/2", lastAccess = 2),
                    createTab(id = "3", url = "https://www.firefox.com")
                )
            )
        )

        val components: Components = mockk(relaxed = true)
        every { components.core.store } returns store

        components.tryReloadTabBy("https://www.getpocket.com".getOrigin()!!)
        verify(exactly = 0) { components.useCases.sessionUseCases.reload(any<String>()) }

        components.tryReloadTabBy("https://www.mozilla.org".getOrigin()!!)
        verify { components.useCases.sessionUseCases.reload("2") }

        components.tryReloadTabBy("https://www.firefox.com".getOrigin()!!)
        verify { components.useCases.sessionUseCases.reload("3") }
    }
}
