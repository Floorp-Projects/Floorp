/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding

import android.annotation.SuppressLint
import android.content.pm.ActivityInfo
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.ViewCompositionStrategy
import androidx.fragment.app.DialogFragment
import androidx.navigation.fragment.findNavController
import com.google.accompanist.insets.ProvideWindowInsets
import mozilla.components.lib.state.ext.observeAsComposableState
import org.mozilla.fenix.R
import org.mozilla.fenix.components.components
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.onboarding.view.UpgradeOnboarding
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Dialog displaying a welcome and sync sign in onboarding.
 */
class HomeOnboardingDialogFragment : DialogFragment() {
    @SuppressLint("SourceLockedOrientationActivity")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setStyle(STYLE_NO_TITLE, R.style.HomeOnboardingDialogStyle)
        activity?.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_PORTRAIT
    }

    override fun onDestroy() {
        super.onDestroy()
        activity?.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View = ComposeView(requireContext()).apply {
        setViewCompositionStrategy(ViewCompositionStrategy.DisposeOnViewTreeLifecycleDestroyed)

        setContent {
            ProvideWindowInsets {
                FirefoxTheme {
                    val account =
                        components.backgroundServices.syncStore.observeAsComposableState { state -> state.account }

                    UpgradeOnboarding(
                        isSyncSignIn = account.value != null,
                        onDismiss = ::onDismiss,
                        onSignInButtonClick = {
                            findNavController().nav(
                                R.id.homeOnboardingDialogFragment,
                                HomeOnboardingDialogFragmentDirections.actionGlobalTurnOnSync(),
                            )
                            onDismiss()
                        },
                    )
                }
            }
        }
    }

    private fun onDismiss() {
        context?.settings()?.showHomeOnboardingDialog = false
        dismiss()
    }
}
