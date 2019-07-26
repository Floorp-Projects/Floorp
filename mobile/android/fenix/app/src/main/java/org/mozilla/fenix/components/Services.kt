/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import androidx.navigation.NavController
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.support.ktx.android.content.hasCamera
import org.mozilla.fenix.Experiments
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.components.features.FirefoxAccountsAuthFeature
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.isInExperiment
import org.mozilla.fenix.test.Mockable

/**
 * Component group which encapsulates foreground-friendly services.
 */
@Mockable
class Services(
    private val accountManager: FxaAccountManager
) {
    val accountsAuthFeature by lazy {
        FirefoxAccountsAuthFeature(
            accountManager,
            redirectUrl = BackgroundServices.REDIRECT_URL
        )
    }

    /**
     * Launches the sign in and pairing custom tab from any screen in the app.
     * @param context the current Context
     * @param navController the navController to use for navigation
     */
    fun launchPairingSignIn(context: Context, navController: NavController) {
        // Do not navigate to pairing UI if camera not available or pairing is disabled
        if (context.hasCamera() && !context.isInExperiment(Experiments.asFeatureFxAPairingDisabled)
        ) {
            val directions = NavGraphDirections.actionGlobalTurnOnSync()
            navController.navigate(directions)
        } else {
            context.components.services.accountsAuthFeature.beginAuthentication(context)
            // TODO The sign-in web content populates session history,
            // so pressing "back" after signing in won't take us back into the settings screen, but rather up the
            // session history stack.
            // We could auto-close this tab once we get to the end of the authentication process?
            // Via an interceptor, perhaps.
            context.components.analytics.metrics.track(Event.SyncAuthSignIn)
        }
    }
}
