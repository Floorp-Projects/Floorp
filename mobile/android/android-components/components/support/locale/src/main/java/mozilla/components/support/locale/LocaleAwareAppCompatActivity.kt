/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.locale

import android.content.Context
import androidx.appcompat.app.AppCompatActivity

/**
 * Base activity for apps that want to customized the system defined language by their own.
 */
open class LocaleAwareAppCompatActivity : AppCompatActivity() {
    override fun attachBaseContext(base: Context) {
        val context = LocaleManager.updateResources(base)
        super.attachBaseContext(context)
    }
}
