/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings.protections.cookiebanners.dialog

import android.content.Context
import androidx.navigation.NavController
import mozilla.components.concept.engine.EngineSession.CookieBannerHandlingStatus
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserFragmentDirections
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.nimbus.CookieBannersSection
import org.mozilla.fenix.nimbus.FxNimbus
import org.mozilla.fenix.utils.Settings

private const val CONTROL_VARIANT = 0
private const val VARIANT_ONE = 1
private const val VARIANT_TWO = 2

/**
 *   An utility object for interacting with the re-engagement cookie banner dialog.
 */
object CookieBannerReEngagementDialogUtils {
    /**
     *   Returns a the current [CookieBannerDialogVariant] to the given nimbus experiment.
     */
    fun getCookieBannerDialogVariants(context: Context): CookieBannerDialogVariant {
        val textVariant =
            FxNimbus.features.cookieBanners.value().sectionsEnabled[CookieBannersSection.DIALOG_TEXT_VARIANT]
        return when (textVariant) {
            CONTROL_VARIANT -> CookieBannerDialogVariant(
                title = context.getString(R.string.reduce_cookie_banner_control_experiment_dialog_title),
                message = context.getString(
                    R.string.reduce_cookie_banner_control_experiment_dialog_body_2,
                    context.getString(
                        R.string.app_name,
                    ),
                ),
                positiveTextButton = context.getString(
                    R.string.reduce_cookie_banner_control_experiment_dialog_change_setting_button,
                ),
            )
            VARIANT_ONE -> CookieBannerDialogVariant(
                title = context.getString(R.string.reduce_cookie_banner_variant_1_experiment_dialog_title),
                message = context.getString(
                    R.string.reduce_cookie_banner_variant_1_experiment_dialog_body_1,
                    context.getString(R.string.app_name),
                ),
                positiveTextButton = context.getString(
                    R.string.reduce_cookie_banner_variant_1_experiment_dialog_change_setting_button,
                ),
            )
            VARIANT_TWO -> CookieBannerDialogVariant(
                title = context.getString(R.string.reduce_cookie_banner_variant_2_experiment_dialog_title),
                message = context.getString(
                    R.string.reduce_cookie_banner_variant_2_experiment_dialog_body_1,
                    context.getString(R.string.app_name),
                ),
                positiveTextButton = context.getString(
                    R.string.reduce_cookie_banner_variant_2_experiment_dialog_change_setting_button,
                ),
            )
            else -> {
                CookieBannerDialogVariant(
                    title = context.getString(R.string.reduce_cookie_banner_control_experiment_dialog_title),
                    message = context.getString(
                        R.string.reduce_cookie_banner_control_experiment_dialog_body_2,
                        context.getString(
                            R.string.app_name,
                        ),
                    ),
                    positiveTextButton = context.getString(
                        R.string.reduce_cookie_banner_control_experiment_dialog_change_setting_button,
                    ),
                )
            }
        }
    }

    /**
     *  Tries to show the re-engagement cookie banner dialog, when the right conditions are met, o
     *  otherwise the dialog won't show.
     */
    fun tryToShowReEngagementDialog(
        settings: Settings,
        status: CookieBannerHandlingStatus,
        navController: NavController,
    ) {
        if (status == CookieBannerHandlingStatus.DETECTED &&
            settings.shouldCookieBannerReEngagementDialog()
        ) {
            settings.lastInteractionWithReEngageCookieBannerDialogInMs = System.currentTimeMillis()
            settings.cookieBannerDetectedPreviously = true
            val directions =
                BrowserFragmentDirections.actionBrowserFragmentToCookieBannerDialogFragment()
            navController.nav(R.id.browserFragment, directions)
        }
    }

    /**
     *  Data class for cookie banner dialog variant
     *  @property title of the dialog
     *  @property message of the dialog
     *  @property positiveTextButton indicates the text of the positive button of the dialog
     */
    data class CookieBannerDialogVariant(
        val title: String,
        val message: String,
        val positiveTextButton: String,
    )
}
