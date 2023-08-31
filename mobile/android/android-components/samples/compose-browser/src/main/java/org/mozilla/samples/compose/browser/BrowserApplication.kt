/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.compose.browser

import android.app.Application
import mozilla.appservices.Megazord
import mozilla.components.feature.fxsuggest.GlobalFxSuggestDependencyProvider
import mozilla.components.support.rusthttp.RustHttpConfig

/**
 * The global [Application] class of this browser application.
 */
class BrowserApplication : Application() {
    val components by lazy { Components(this) }

    override fun onCreate() {
        super.onCreate()

        Megazord.init()
        RustHttpConfig.setClient(lazy { components.client })

        GlobalFxSuggestDependencyProvider.initialize(components.fxSuggestStorage)
    }
}
