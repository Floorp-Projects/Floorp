/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser

import android.content.Context
import mozilla.components.engine.Engine
import mozilla.components.engine.system.SystemEngine
import mozilla.components.session.SessionManager
import mozilla.components.session.SessionProvider
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoRuntimeSettings

/**
 * Helper class for lazily instantiating components needed by the application.
 */
class Components(private val applcationContext: Context) {
    private val geckoRuntime by lazy {
        val settings = GeckoRuntimeSettings()
        GeckoRuntime.create(applcationContext, settings)
    }

    private val sessionProvider : SessionProvider by lazy { DummySessionProvider() }

    //val engine : Engine by lazy { GeckoEngine(geckoRuntime) }
    val engine : Engine by lazy { SystemEngine() }

    val sessionManager : SessionManager by lazy { SessionManager(sessionProvider) }
}
