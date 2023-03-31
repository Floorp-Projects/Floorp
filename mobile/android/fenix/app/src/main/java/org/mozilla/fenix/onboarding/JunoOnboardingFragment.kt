/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding

import android.annotation.SuppressLint
import android.content.Context
import android.content.pm.ActivityInfo
import android.os.Build
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.annotation.RequiresApi
import androidx.compose.runtime.Composable
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.ViewCompositionStrategy
import androidx.core.app.NotificationManagerCompat
import androidx.fragment.app.Fragment
import androidx.navigation.fragment.findNavController
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.areNotificationsEnabledSafe
import org.mozilla.fenix.ext.hideToolbar
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.openSetDefaultBrowserOption
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.onboarding.view.JunoOnboardingPageType
import org.mozilla.fenix.onboarding.view.JunoOnboardingScreen
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Fragment displaying the juno onboarding flow.
 */
class JunoOnboardingFragment : Fragment() {

    private val fenixOnboarding by lazy { FenixOnboarding(requireContext()) }
    private val onboardingPageTypeList by lazy { onboardingPageTypeList(requireContext()) }

    @SuppressLint("SourceLockedOrientationActivity")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        activity?.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_PORTRAIT
    }

    @RequiresApi(Build.VERSION_CODES.TIRAMISU)
    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View = ComposeView(requireContext()).apply {
        setViewCompositionStrategy(ViewCompositionStrategy.DisposeOnViewTreeLifecycleDestroyed)
        setContent {
            FirefoxTheme {
                ScreenContent()
            }
        }
    }

    override fun onResume() {
        super.onResume()
        hideToolbar()
    }

    override fun onDestroy() {
        super.onDestroy()
        activity?.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED
    }

    @RequiresApi(Build.VERSION_CODES.TIRAMISU)
    @Composable
    private fun ScreenContent() {
        val context = LocalContext.current
        JunoOnboardingScreen(
            onboardingPageTypeList = onboardingPageTypeList,
            onMakeFirefoxDefaultClick = { activity?.openSetDefaultBrowserOption(useCustomTab = true) },
            onPrivacyPolicyClick = { url ->
                startActivity(SupportUtils.createSandboxCustomTabIntent(context = context, url = url))
            },
            onSignInButtonClick = {
                findNavController().nav(
                    id = R.id.junoOnboardingFragment,
                    directions = JunoOnboardingFragmentDirections.actionGlobalTurnOnSync(),
                )
            },
            onNotificationPermissionButtonClick = {
                requireComponents.notificationsDelegate.requestNotificationPermission()
            },
            onFinish = { onFinish() },
        )
    }

    private fun onFinish() {
        context?.settings()?.isJunoOnboardingShown = true
        fenixOnboarding.finish()
        findNavController().nav(
            id = R.id.junoOnboardingFragment,
            directions = JunoOnboardingFragmentDirections.actionOnboardingHome(),
        )
    }

    private fun onboardingPageTypeList(context: Context): List<JunoOnboardingPageType> =
        buildList {
            add(JunoOnboardingPageType.DEFAULT_BROWSER)
            add(JunoOnboardingPageType.SYNC_SIGN_IN)
            if (shouldShowNotificationPage(context)) {
                add(JunoOnboardingPageType.NOTIFICATION_PERMISSION)
            }
        }

    private fun shouldShowNotificationPage(context: Context) =
        !NotificationManagerCompat.from(context.applicationContext).areNotificationsEnabledSafe() &&
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU
}
