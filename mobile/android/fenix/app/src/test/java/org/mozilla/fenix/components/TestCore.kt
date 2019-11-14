/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import io.mockk.mockk
import mozilla.components.browser.engine.gecko.GeckoEngine
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.pwa.WebAppShortcutManager
import mozilla.components.lib.dataprotect.SecureAbove22Preferences

class TestCore(context: Context) : Core(context) {

    override val engine = mockk<GeckoEngine>(relaxed = true)
    override val sessionManager = SessionManager(engine)
    override val store = mockk<BrowserStore>()
    override val webAppShortcutManager = mockk<WebAppShortcutManager>()
    override fun getSecureAbove22Preferences() = mockk<SecureAbove22Preferences>(relaxed = true)
}
