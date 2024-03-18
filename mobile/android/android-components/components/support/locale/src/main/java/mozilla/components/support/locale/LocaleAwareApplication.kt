/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.locale

import android.app.Application
import android.content.Context
import android.content.res.Configuration

/**
 * Base application for apps that want to customized the system defined language by their own.
 */
open class LocaleAwareApplication : Application() {

    override fun attachBaseContext(base: Context) {
        val context = LocaleManager.updateResources(base)
        super.attachBaseContext(context)
    }

    override fun onConfigurationChanged(config: Configuration) {
        super.onConfigurationChanged(config)
        LocaleManager.updateResources(this)
    }
}
