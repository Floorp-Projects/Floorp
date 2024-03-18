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
import androidx.activity.result.ActivityResult
import androidx.activity.result.ActivityResultLauncher
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.utils.Browsers
import mozilla.components.support.utils.ext.navigateToDefaultBrowserAppsSettings
import org.mozilla.focus.ext.settings
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.state.AppStore

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
                            Logger(TAG).error(
                                "ActivityNotFoundException " +
                                    e.message.toString(),
                            )
                            handleFinishOnBoarding()
                        }
                    } else {
                        context.navigateToDefaultBrowserAppsSettings()
                    }
                }
            }
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.N -> {
                context.navigateToDefaultBrowserAppsSettings()
            }
        }
    }

    companion object {
        const val TAG = "OnboardingController"
    }
}
