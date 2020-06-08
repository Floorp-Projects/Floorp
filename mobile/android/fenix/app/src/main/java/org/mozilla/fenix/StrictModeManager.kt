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
         *  Enables strict mode for debug purposes. meant to be run only in the main process.
         *  @param setPenaltyDialog boolean value to decide setting the dialog box as  a penalty.
         */
        fun enableStrictMode(setPenaltyDialog: Boolean) {
            if (Config.channel.isDebug) {
                val threadPolicy = StrictMode.ThreadPolicy.Builder()
                    .detectAll()
                    .penaltyLog()
                if (setPenaltyDialog) {
                    threadPolicy.penaltyDialog()
                }
                StrictMode.setThreadPolicy(threadPolicy.build())
                var builder = StrictMode.VmPolicy.Builder()
                    .detectLeakedSqlLiteObjects()
                    .detectLeakedClosableObjects()
                    .detectLeakedRegistrationObjects()
                    .detectActivityLeaks()
                    .detectFileUriExposure()
                    .penaltyLog()
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) builder =
                    builder.detectContentUriWithoutPermission()
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                    if (setPenaltyDialog) {
                        builder.permitNonSdkApiUsage()
                    } else {
                        builder.detectNonSdkApiUsage()
                    }
                }
                StrictMode.setVmPolicy(builder.build())
            }
        }

    /**
     * Revert strict mode to disable penalty dialog. Tied to fragment lifecycle since strict mode
     * needs to switch to penalty logs. Using the fragment life cycle allows decoupling from any
     * specific fragment.
     */
    fun changeStrictModePolicies(fragmentManager: FragmentManager) {
        fragmentManager.registerFragmentLifecycleCallbacks(object :
            FragmentManager.FragmentLifecycleCallbacks() {
            override fun onFragmentResumed(fm: FragmentManager, f: Fragment) {
                enableStrictMode(false)
                fm.unregisterFragmentLifecycleCallbacks(this)
            }
        }, false)
    }
}
