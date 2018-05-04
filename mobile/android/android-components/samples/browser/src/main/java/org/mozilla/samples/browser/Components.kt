/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser

import android.content.Context
import mozilla.components.browser.engine.gecko.GeckoEngine
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.SessionProvider
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.session.SessionIntentProcessor
import mozilla.components.feature.session.SessionMapping
import mozilla.components.feature.session.SessionUseCases
import org.mozilla.geckoview.GeckoRuntime

/**
 * Helper class for lazily instantiating components needed by the application.
 */
class Components(private val applicationContext: Context) {
    private val geckoRuntime by lazy {
        GeckoRuntime.getDefault(applicationContext)
    }

    val engine : Engine by lazy { GeckoEngine(geckoRuntime) }
    // val engine : Engine by lazy { SystemEngine() }

    private val sessionProvider : SessionProvider by lazy { DummySessionProvider() }
    val sessionManager : SessionManager by lazy { SessionManager(sessionProvider) }
    val sessionMapping = SessionMapping()
    val sessionUseCases = SessionUseCases(sessionManager, engine, sessionMapping)
    val sessionIntentProcessor = SessionIntentProcessor(sessionUseCases)
}
