/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser

import android.content.Context
import mozilla.components.browser.engine.gecko.GeckoEngine
import mozilla.components.concept.engine.DefaultSettings
import mozilla.components.concept.engine.Engine
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.samples.browser.request.SampleRequestInterceptor

/**
 * Helper class for lazily instantiating components needed by the application.
 */
class Components(applicationContext: Context) : DefaultComponents(applicationContext){
    override val engine: Engine by lazy {
        val defaultSettings = DefaultSettings().apply {
            requestInterceptor = SampleRequestInterceptor()
        }

        val runtime = GeckoRuntime.getDefault(applicationContext)
        GeckoEngine(runtime, defaultSettings)
    }
}
