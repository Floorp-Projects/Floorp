/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.os.Build
import android.os.StrictMode
import androidx.fragment.app.Fragment
import androidx.fragment.app.FragmentManager
import mozilla.components.support.ktx.android.os.resetAfter

private const val MANUFACTURE_HUAWEI: String = "HUAWEI"
private const val MANUFACTURE_ONE_PLUS: String = "OnePlus"

/**
 * Manages strict mode settings for the application.
 */
class StrictModeManager(config: Config) {

    // This is public so it can be used by inline functions.
    val isEnabledByBuildConfig = config.channel.isDebug

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
     * Runs the given [functionBlock] and sets the given [StrictMode.ThreadPolicy] after its
     * completion when in a build configuration that has StrictMode enabled. If StrictMode is
     * not enabled, simply runs the [functionBlock].
     *
     * This function is written in the style of [AutoCloseable.use].
     *
     * This is significantly less convenient to run than when it was written as an extension function
     * on [StrictMode.ThreadPolicy] but I think this is okay: it shouldn't be easy to ignore StrictMode.
     *
     * @return the value returned by [functionBlock].
     */
    inline fun <R> resetAfter(policy: StrictMode.ThreadPolicy, functionBlock: () -> R): R {
        // Calling resetAfter takes 1-2ms (unknown device) so we only execute it if StrictMode can
        // actually be enabled. https://github.com/mozilla-mobile/fenix/issues/11617
        return if (isEnabledByBuildConfig) {
            policy.resetAfter(functionBlock)
        } else {
            functionBlock()
        }
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
