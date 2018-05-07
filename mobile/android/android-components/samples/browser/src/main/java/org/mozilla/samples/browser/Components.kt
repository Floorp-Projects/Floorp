/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser

import android.content.Context
import mozilla.components.browser.engine.gecko.GeckoEngine
import mozilla.components.browser.session.Session
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.session.SessionIntentProcessor
import mozilla.components.feature.session.SessionProvider
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

    val sessionProvider = SessionProvider(applicationContext, Session("https://www.mozilla.org"))
    val sessionUseCases = SessionUseCases(sessionProvider, engine)
    val sessionIntentProcessor = SessionIntentProcessor(sessionUseCases)
}
