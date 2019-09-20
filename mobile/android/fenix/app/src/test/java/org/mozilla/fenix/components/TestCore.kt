/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import io.mockk.mockk
import kotlinx.coroutines.ObsoleteCoroutinesApi
import mozilla.components.browser.engine.gecko.GeckoEngine
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.fetch.Client

@ObsoleteCoroutinesApi
class TestCore(private val context: Context) : Core(context) {

    override val engine = mockk<GeckoEngine>(relaxed = true)
    override val sessionManager = SessionManager(engine)
    override val client = mockk<Client>()
}
