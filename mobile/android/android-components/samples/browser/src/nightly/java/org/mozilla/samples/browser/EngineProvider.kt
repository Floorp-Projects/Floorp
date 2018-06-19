/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser

import android.content.Context
import mozilla.components.browser.engine.gecko.GeckoEngine
import mozilla.components.concept.engine.Engine
import org.mozilla.geckoview.GeckoRuntime

object EngineProvider {
    fun createEngine(applicationContext: Context): Engine {
        val runtime = GeckoRuntime.getDefault(applicationContext)
        return GeckoEngine(runtime)
    }
}
