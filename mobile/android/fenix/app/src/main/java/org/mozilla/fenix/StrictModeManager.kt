/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.os.Build
import android.os.StrictMode
import androidx.fragment.app.Fragment
import androidx.fragment.app.FragmentManager

private const val MANUFACTURE_HUAWEI: String = "HUAWEI"
private const val MANUFACTURE_ONE_PLUS: String = "OnePlus"

/**
 * Manages strict mode settings for the application.
 */
class StrictModeManager(config: Config) {

    // The expression in this if is duplicated in StrictMode.ThreadPolicy.resetPoliciesAfter
    // because we don't want to have to pass in a dependency each time the ext fn is called.
    private val isEnabledByBuildConfig = config.channel.isDebug

    /***
     * Enables strict mode for debug purposes. meant to be run only in the main process.
     * @param setPenaltyDeath boolean value to decide setting the penaltyDeath as a penalty.
     */
    fun enableStrictMode(setPenaltyDeath: Boolean) {
        if (isEnabledByBuildConfig) {
            val threadPolicy = StrictMode.ThreadPolicy.Builder()
                .detectAll()
                .penaltyLog()
            if (setPenaltyDeath && Build.MANUFACTURER !in strictModeExceptionList) {
                threadPolicy.penaltyDeath()
            }
            StrictMode.setThreadPolicy(threadPolicy.build())

            val builder = StrictMode.VmPolicy.Builder()
                .detectLeakedSqlLiteObjects()
                .detectLeakedClosableObjects()
                .detectLeakedRegistrationObjects()
                .detectActivityLeaks()
                .detectFileUriExposure()
                .penaltyLog()
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                builder.detectContentUriWithoutPermission()
            }
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                builder.detectNonSdkApiUsage()
            }
            StrictMode.setVmPolicy(builder.build())
        }
    }

    /**
     * Revert strict mode to disable penalty based on fragment lifecycle since strict mode
     * needs to switch to penalty logs. Using the fragment life cycle allows decoupling from any
     * specific fragment.
     */
    fun attachListenerToDisablePenaltyDeath(fragmentManager: FragmentManager) {
        fragmentManager.registerFragmentLifecycleCallbacks(object :
            FragmentManager.FragmentLifecycleCallbacks() {
            override fun onFragmentResumed(fm: FragmentManager, f: Fragment) {
                enableStrictMode(setPenaltyDeath = false)
                fm.unregisterFragmentLifecycleCallbacks(this)
            }
        }, false)
    }

    /**
     * There are certain manufacturers that have custom font classes for the OS systems.
     * These classes violates the [StrictMode] policies on startup. As a workaround, we create
     * an exception list for these manufacturers so that dialogs do not show up on start up.
     * To add a new manufacturer to the list, log "Build.MANUFACTURER" from the device to get the
     * exact name of the manufacturer.
     */
    private val strictModeExceptionList = setOf(MANUFACTURE_HUAWEI, MANUFACTURE_ONE_PLUS)
}
