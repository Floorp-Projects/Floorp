/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shortcut

import androidx.annotation.VisibleForTesting
import androidx.lifecycle.LifecycleOwner
import androidx.navigation.NavController
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.distinctUntilChangedBy
import kotlinx.coroutines.flow.mapNotNull
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.pwa.WebAppUseCases
import mozilla.components.lib.state.ext.flowScoped
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserFragmentDirections
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.utils.Settings

/**
 * Displays the [PwaOnboardingDialogFragment] info dialog when a PWA is opened in the browser for the third time.
 */
class PwaOnboardingObserver(
    private val store: BrowserStore,
    private val lifecycleOwner: LifecycleOwner,
    private val navController: NavController,
    private val settings: Settings,
    private val webAppUseCases: WebAppUseCases,
) {

    private var scope: CoroutineScope? = null

    fun start() {
        scope = store.flowScoped(lifecycleOwner) { flow ->
            flow.mapNotNull { state ->
                state.selectedTab
            }
                .distinctUntilChangedBy {
                    it.content.webAppManifest
                }
                .collect {
                    if (webAppUseCases.isInstallable() && !settings.userKnowsAboutPwas) {
                        settings.incrementVisitedInstallableCount()
                        if (settings.shouldShowPwaCfr) {
                            navigateToPwaOnboarding()
                            settings.lastCfrShownTimeInMillis = System.currentTimeMillis()
                            settings.userKnowsAboutPwas = true
                        }
                    }
                }
        }
    }

    fun stop() {
        scope?.cancel()
    }

    @VisibleForTesting
    internal fun navigateToPwaOnboarding() {
        navController.nav(
            R.id.browserFragment,
            BrowserFragmentDirections.actionBrowserFragmentToPwaOnboardingDialogFragment(),
        )
    }
}
