/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import android.content.Context
import androidx.fragment.app.Fragment
import androidx.navigation.NavController
import mozilla.components.browser.state.state.SessionState
import mozilla.components.concept.engine.permission.SitePermissions
import org.mozilla.fenix.browser.BrowserFragmentDirections
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.runIfFragmentIsAttached

/**
 * [ConnectionDetailsController] controller.
 *
 * Delegated by View Interactors, handles container business logic and operates changes on it,
 * complex Android interactions or communication with other features.
 */
interface ConnectionDetailsController {
    /**
     * @see [WebSiteInfoInteractor.onBackPressed]
     */
    fun handleBackPressed()
}

/**
 * Default behavior of [ConnectionDetailsController].
 */
class DefaultConnectionDetailsController(
    private val context: Context,
    private val fragment: Fragment,
    private val navController: () -> NavController,
    internal var sitePermissions: SitePermissions?,
    private val gravity: Int,
    private val getCurrentTab: () -> SessionState?
) : ConnectionDetailsController {

    override fun handleBackPressed() {
        getCurrentTab()?.let { tab ->
            context.components.useCases.trackingProtectionUseCases.containsException(tab.id) { contains ->
                fragment.runIfFragmentIsAttached {
                    navController().popBackStack()
                    val isTrackingProtectionEnabled = tab.trackingProtection.enabled && !contains
                    val directions =
                        BrowserFragmentDirections.actionGlobalQuickSettingsSheetDialogFragment(
                            sessionId = tab.id,
                            url = tab.content.url,
                            title = tab.content.title,
                            isSecured = tab.content.securityInfo.secure,
                            sitePermissions = sitePermissions,
                            gravity = gravity,
                            certificateName = tab.content.securityInfo.issuer,
                            permissionHighlights = tab.content.permissionHighlights,
                            isTrackingProtectionEnabled = isTrackingProtectionEnabled
                        )
                    navController().navigate(directions)
                }
            }
        }
    }
}
