/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.webextensions

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.action.WebExtensionAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.WebExtensionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class WebExtensionPopupFeatureTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `observes and forwards request to open popup`() {
        val extensionId = "ext1"
        val engineSession: EngineSession = mock()
        val store = BrowserStore(
            BrowserState(
                extensions = mapOf(extensionId to WebExtensionState(extensionId)),
            ),
        )

        var extensionOpeningPopup: WebExtensionState? = null
        val feature = WebExtensionPopupFeature(store) {
            extensionOpeningPopup = it
        }

        feature.start()
        assertNull(extensionOpeningPopup)

        store.dispatch(WebExtensionAction.UpdatePopupSessionAction(extensionId, popupSession = engineSession)).joinBlocking()
        assertNotNull(extensionOpeningPopup)
        assertEquals(extensionId, extensionOpeningPopup!!.id)
        assertEquals(engineSession, extensionOpeningPopup!!.popupSession)

        // Verify that stopped feature does not observe and forward requests to open popup
        extensionOpeningPopup = null
        feature.stop()
        store.dispatch(WebExtensionAction.UpdatePopupSessionAction(extensionId, popupSession = mock())).joinBlocking()
        assertNull(extensionOpeningPopup)
    }
}
