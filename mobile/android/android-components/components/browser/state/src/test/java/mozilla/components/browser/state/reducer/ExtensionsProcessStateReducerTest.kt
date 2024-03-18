/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.ExtensionsProcessAction
import mozilla.components.browser.state.state.BrowserState
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class ExtensionsProcessStateReducerTest {
    @Test
    fun `GIVEN ShowPromptAction THEN showExtensionsProcessDisabledPrompt is updated`() {
        var state = BrowserState()
        assertFalse(state.showExtensionsProcessDisabledPrompt)

        state = ExtensionsProcessStateReducer.reduce(state, ExtensionsProcessAction.ShowPromptAction(show = true))
        assertTrue(state.showExtensionsProcessDisabledPrompt)

        state = ExtensionsProcessStateReducer.reduce(state, ExtensionsProcessAction.ShowPromptAction(show = false))
        assertFalse(state.showExtensionsProcessDisabledPrompt)
    }

    @Test
    fun `GIVEN EnabledAction THEN extensionsProcessDisabled is set to false`() {
        var state = BrowserState(extensionsProcessDisabled = true)

        state = ExtensionsProcessStateReducer.reduce(state, ExtensionsProcessAction.EnabledAction)
        assertFalse(state.extensionsProcessDisabled)
    }

    @Test
    fun `GIVEN DisabledAction THEN extensionsProcessDisabled is set to true`() {
        var state = BrowserState(extensionsProcessDisabled = false)

        state = ExtensionsProcessStateReducer.reduce(state, ExtensionsProcessAction.DisabledAction)
        assertTrue(state.extensionsProcessDisabled)
    }
}
