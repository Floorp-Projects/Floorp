/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.fragment.onboarding

import android.app.Activity
import android.app.role.RoleManager
import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import android.os.Build
import android.provider.Settings
import androidx.activity.result.ActivityResult
import androidx.activity.result.ActivityResultLauncher
import androidx.annotation.VisibleForTesting
import androidx.core.os.bundleOf
import mozilla.components.feature.search.widget.BaseVoiceSearchActivity
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.utils.Browsers
import org.mozilla.focus.ext.settings
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.state.AppStore
import org.mozilla.focus.utils.ManufacturerCodes
import org.mozilla.focus.utils.SupportUtils
import org.mozilla.focus.widget.DefaultBrowserPreference

interface OnboardingController {
    fun handleFinishOnBoarding()
    fun handleGetStartedButtonClicked()
    fun handleMakeFocusDefaultBrowserButtonClicked(activityResultLauncher: ActivityResultLauncher<Intent>)
    fun handleActivityResultImplementation(activityResult: ActivityResult)
}

class DefaultOnboardingController(
    private val onboardingStorage: OnboardingStorage,
    val appStore: AppStore,
    val context: Context,
    val selectedTabId: String?,
) : OnboardingController {

    override fun handleFinishOnBoarding() {
        context.settings.isFirstRun = false
        appStore.dispatch(AppAction.FinishFirstRun(selectedTabId))
    }

    override fun handleGetStartedButtonClicked() {
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.M || Browsers.all(context).isDefaultBrowser) {
            handleFinishOnBoarding()
        } else {
            navigateToOnBoardingSecondScreen()
        }
    }

    override fun handleMakeFocusDefaultBrowserButtonClicked(activityResultLauncher: ActivityResultLauncher<Intent>) {
        if (ManufacturerCodes.isHuawei) {
            SupportUtils.openDefaultBrowserSumoPage(context)
            handleFinishOnBoarding()
            return
        }
        val isDefault = Browsers.all(context).isDefaultBrowser
        if (isDefault) {
            handleFinishOnBoarding()
        } else {
            makeFocusDefaultBrowser(activityResultLauncher)
        }
    }

    override fun handleActivityResultImplementation(activityResult: ActivityResult) {
        if (activityResult.resultCode == Activity.RESULT_OK && Browsers.all(context).isDefaultBrowser) {
            handleFinishOnBoarding()
        }
    }

    private fun navigateToOnBoardingSecondScreen() {
        onboardingStorage.saveCurrentOnboardingStepInSharePref(OnboardingStep.ON_BOARDING_SECOND_SCREEN)
        appStore.dispatch(AppAction.ShowOnboardingSecondScreen)
    }

    private fun makeFocusDefaultBrowser(activityResultLauncher: ActivityResultLauncher<Intent>) {
        when {
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q -> {
                context.getSystemService(RoleManager::class.java).also {
                    if (
                        it.isRoleAvailable(RoleManager.ROLE_BROWSER) &&
                        !it.isRoleHeld(RoleManager.ROLE_BROWSER)
                    ) {
                        try {
                            activityResultLauncher.launch(it.createRequestRoleIntent(RoleManager.ROLE_BROWSER))
                        } catch (e: ActivityNotFoundException) {
                            Logger(BaseVoiceSearchActivity.TAG).error(
                                "ActivityNotFoundException " +
                                    e.message.toString(),
                            )
                            handleFinishOnBoarding()
                        }
                    } else {
                        navigateToDefaultBrowserAppsSettings()
                    }
                }
            }
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.N -> {
                navigateToDefaultBrowserAppsSettings()
            }
        }
    }

    @VisibleForTesting
    fun navigateToDefaultBrowserAppsSettings() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            val intent = Intent(Settings.ACTION_MANAGE_DEFAULT_APPS_SETTINGS).apply {
                this.putExtra(
                    DefaultBrowserPreference.SETTINGS_SELECT_OPTION_KEY,
                    DefaultBrowserPreference.DEFAULT_BROWSER_APP_OPTION,
                )
                this.putExtra(
                    DefaultBrowserPreference.SETTINGS_SHOW_FRAGMENT_ARGS,
                    bundleOf(
                        DefaultBrowserPreference.SETTINGS_SELECT_OPTION_KEY to
                            DefaultBrowserPreference.DEFAULT_BROWSER_APP_OPTION,
                    ),
                )
            }
            context.startActivity(intent)
        }
    }
}
