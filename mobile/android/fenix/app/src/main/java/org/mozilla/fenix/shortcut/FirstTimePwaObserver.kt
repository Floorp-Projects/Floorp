/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shortcut

import androidx.navigation.NavController
import mozilla.components.browser.session.Session
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.feature.pwa.WebAppUseCases
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserFragmentDirections
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.utils.Settings

/**
 * Displays the [FirstTimePwaFragment] info dialog when a PWA is first opened in the browser.
 */
class FirstTimePwaObserver(
    private val navController: NavController,
    private val settings: Settings,
    private val webAppUseCases: WebAppUseCases
) : Session.Observer {

    override fun onWebAppManifestChanged(session: Session, manifest: WebAppManifest?) {
        if (webAppUseCases.isInstallable() && settings.shouldShowFirstTimePwaFragment) {
            val directions = BrowserFragmentDirections.actionBrowserFragmentToFirstTimePwaFragment()
            navController.nav(R.id.browserFragment, directions)

            settings.userKnowsAboutPWAs = true
        }
    }
}
