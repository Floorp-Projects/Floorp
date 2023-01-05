/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings.protections.cookiebanners.dialog

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.compose.ui.platform.ComposeView
import androidx.fragment.app.DialogFragment
import mozilla.components.concept.engine.EngineSession.CookieBannerHandlingMode.REJECT_OR_ACCEPT_ALL
import mozilla.components.concept.engine.Settings
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.GleanMetrics.CookieBanners
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getRootView
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Displays a cookie banner dialog fragment that contains the dialog compose and his logic.
 */
class CookieBannerReEngagementDialog : DialogFragment() {

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View = ComposeView(requireContext()).apply {
        CookieBanners.visitedReEngagementDialog.record(NoExtras())

        setContent {
            FirefoxTheme {
                val cookieBannerDialogSelectedVariant =
                    CookieBannerReEngagementDialogUtils.getCookieBannerDialogVariants(requireContext())
                CookieBannerReEngagementDialogCompose(
                    dialogTitle = cookieBannerDialogSelectedVariant.title,
                    dialogText = cookieBannerDialogSelectedVariant.message,
                    allowButtonText = cookieBannerDialogSelectedVariant.positiveTextButton,
                    declineButtonText = getString(R.string.reduce_cookie_banner_dialog_not_now_button),
                    onAllowButtonClicked = {
                        CookieBanners.allowReEngagementDialog.record(NoExtras())
                        requireContext().settings().shouldUseCookieBanner = true
                        getEngineSettings().cookieBannerHandlingModePrivateBrowsing = REJECT_OR_ACCEPT_ALL
                        getEngineSettings().cookieBannerHandlingMode = REJECT_OR_ACCEPT_ALL
                        reload()
                        requireContext().getRootView()?.let {
                            FenixSnackbar.make(
                                view = it,
                                duration = FenixSnackbar.LENGTH_LONG,
                                isDisplayedWithBrowserToolbar = true,
                            )
                                .setText(getString(R.string.reduce_cookie_banner_dialog_snackbar_text))
                                .show()
                        }
                        dismiss()
                    },
                    onNotNowButtonClicked = {
                        CookieBanners.notNowReEngagementDialog.record(NoExtras())
                        dismiss()
                    },
                    onCloseButtonClicked = {
                        requireContext().settings().userOptOutOfReEngageCookieBannerDialog = true
                        CookieBanners.optOutReEngagementDialog.record(NoExtras())
                        dismiss()
                    },
                )
            }
        }
    }

    private fun getEngineSettings(): Settings {
        return requireContext().components.core.engine.settings
    }

    private fun reload() {
        return requireContext().components.useCases.sessionUseCases.reload()
    }
}
