/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.tab

import android.app.Service
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.os.IBinder
import android.support.customtabs.ICustomTabsCallback
import android.support.customtabs.ICustomTabsService

class CustomTabsService : Service() {
    override fun onBind(intent: Intent): IBinder? {
        return object : ICustomTabsService.Stub() {
            override fun warmup(flags: Long): Boolean = true
            override fun newSession(cb: ICustomTabsCallback) = true
            override fun mayLaunchUrl(cb: ICustomTabsCallback, url: Uri, extras: Bundle, bundles: List<Bundle>) = true
            override fun extraCommand(s: String, bundle: Bundle): Bundle? = null
            override fun updateVisuals(cb: ICustomTabsCallback, bundle: Bundle) = false
            override fun requestPostMessageChannel(cb: ICustomTabsCallback, uri: Uri): Boolean = false
            override fun postMessage(cb: ICustomTabsCallback, s: String, bundle: Bundle): Int = 0
            override fun validateRelationship(cb: ICustomTabsCallback, i: Int, uri: Uri, bundle: Bundle) = false
        }
    }
}
