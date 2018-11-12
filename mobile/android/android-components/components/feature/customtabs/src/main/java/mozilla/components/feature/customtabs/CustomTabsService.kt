/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.customtabs

import android.app.Service
import android.net.Uri
import android.os.Bundle
import android.support.customtabs.CustomTabsSessionToken

/**
 * [Service] providing Custom Tabs related functionality.
 */
class CustomTabsService : android.support.customtabs.CustomTabsService() {
    override fun warmup(flags: Long): Boolean {
        return true
    }

    override fun requestPostMessageChannel(sessionToken: CustomTabsSessionToken?, postMessageOrigin: Uri?): Boolean {
        return false
    }

    override fun newSession(sessionToken: CustomTabsSessionToken?): Boolean {
        return true
    }

    override fun extraCommand(commandName: String?, args: Bundle?): Bundle? {
        return null
    }

    override fun mayLaunchUrl(
        sessionToken: CustomTabsSessionToken?,
        url: Uri?,
        extras: Bundle?,
        otherLikelyBundles: MutableList<Bundle>?
    ): Boolean {
        return true
    }

    override fun postMessage(sessionToken: CustomTabsSessionToken?, message: String?, extras: Bundle?): Int {
        return 0
    }

    override fun validateRelationship(
        sessionToken: CustomTabsSessionToken?,
        relation: Int,
        origin: Uri?,
        extras: Bundle?
    ): Boolean {
        return false
    }

    override fun updateVisuals(sessionToken: CustomTabsSessionToken?, bundle: Bundle?): Boolean {
        return false
    }
}
