/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.prompt

import mozilla.components.browser.engine.gecko.webextension.GeckoWebExtension
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.webextension.MessageHandler
import mozilla.components.support.test.mock
import org.junit.Assert.assertFalse
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class GeckoWebExtensionTest {

    // There is not much to test here in beta. All functionality is in nightly only.
    @Test
    fun `create gecko web extension`() {
        val session: EngineSession = mock()
        val contentMessageHandler: MessageHandler = mock()
        val backgroundMessageHandler: MessageHandler = mock()
        val appName = "mozac-test"

        val ext = GeckoWebExtension(appName, "url")

        ext.registerBackgroundMessageHandler(appName, backgroundMessageHandler)
        ext.registerContentMessageHandler(session, appName, contentMessageHandler)
        assertFalse(ext.hasContentMessageHandler(session, appName))
    }
}