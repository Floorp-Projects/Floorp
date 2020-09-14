/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.os.Build
import android.os.StrictMode
import androidx.fragment.app.Fragment
import androidx.fragment.app.FragmentManager

/**
 * Manages strict mode settings for the application.
 */
object StrictModeManager {

    /***
     * Enables strict mode for debug purposes. meant to be run only in the main process.
     * @param setPenaltyDeath boolean value to decide setting the penaltyDeath as a penalty.
     * @param setPenaltyDialog boolean value to decide setting the dialog box as a penalty.
     * Note: dialog penalty cannot be set with penaltyDeath
     */
    fun enableStrictMode(setPenaltyDeath: Boolean, setPenaltyDialog: Boolean = false) {
        if (Config.channel.isDebug) {
            val threadPolicy = StrictMode.ThreadPolicy.Builder()
                .detectAll()
                .penaltyLog()
            if (setPenaltyDeath && Build.MANUFACTURER !in strictModeExceptionList) {
                threadPolicy.penaltyDeath()
            }

            // dialog penalty cannot be set with penaltyDeath
            if (!setPenaltyDeath && setPenaltyDialog) {
                threadPolicy.penaltyDialog()
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
                if (setPenaltyDeath || setPenaltyDialog) {
                    builder.permitNonSdkApiUsage()
                } else {
                    builder.detectNonSdkApiUsage()
                }
            }
            StrictMode.setVmPolicy(builder.build())
        }
    }

    /**
     * Revert strict mode to disable penalty. Tied to fragment lifecycle since strict mode
     * needs to switch to penalty logs. Using the fragment life cycle allows decoupling from any
     * specific fragment.
     */
    fun changeStrictModePolicies(fragmentManager: FragmentManager) {
        fragmentManager.registerFragmentLifecycleCallbacks(object :
            FragmentManager.FragmentLifecycleCallbacks() {
            override fun onFragmentResumed(fm: FragmentManager, f: Fragment) {
                enableStrictMode(setPenaltyDeath = false, setPenaltyDialog = false)
                fm.unregisterFragmentLifecycleCallbacks(this)
            }
        }, false)
    }

    private const val MANUFACTURE_HUAWEI: String = "HUAWEI"
    private const val MANUFACTURE_ONE_PLUS: String = "OnePlus"

    /**
     * There are certain manufacturers that have custom font classes for the OS systems.
     * These classes violates the [StrictMode] policies on startup. As a workaround, we create
     * an exception list for these manufacturers so that dialogs do not show up on start up.
     * To add a new manufacturer to the list, log "Build.MANUFACTURER" from the device to get the
     * exact name of the manufacturer.
     */
    private val strictModeExceptionList = setOf(MANUFACTURE_HUAWEI, MANUFACTURE_ONE_PLUS)
}
