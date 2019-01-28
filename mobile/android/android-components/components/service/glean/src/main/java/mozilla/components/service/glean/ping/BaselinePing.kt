/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.ping

import android.view.accessibility.AccessibilityManager
import android.accessibilityservice.AccessibilityServiceInfo
import android.content.Context
import android.os.Build
import mozilla.components.service.glean.GleanMetrics.GleanBaseline
import mozilla.components.support.base.log.logger.Logger

/**
 * BaselinePing facilitates setup and initialization of baseline pings.
 */
internal class BaselinePing(applicationContext: Context) {
    private val logger = Logger("glean/BaselinePing")

    companion object {
        const val STORE_NAME = "baseline"
    }

    init {
        // Set the OS type
        GleanBaseline.os.set("Android")

        // Set the OS version
        // https://developer.android.com/reference/android/os/Build.VERSION
        GleanBaseline.osVersion.set(Build.VERSION.SDK_INT.toString())

        // Set the device strings
        // https://developer.android.com/reference/android/os/Build
        GleanBaseline.deviceManufacturer.set(Build.MANUFACTURER)
        GleanBaseline.deviceModel.set(Build.MODEL)

        // Set the CPU architecture
        GleanBaseline.architecture.set(Build.SUPPORTED_ABIS[0])

        // Set the enabled accessibility services
        getEnabledAccessibilityServices(
            applicationContext.getSystemService(Context.ACCESSIBILITY_SERVICE) as AccessibilityManager
        ) ?.let {
            GleanBaseline.a11yServices.set(it)
        }
    }

    /**
     * Records a list of the currently enabled accessibility services.
     *
     * https://developer.android.com/reference/android/view/accessibility/AccessibilityManager.html
     * @param accessibilityManager The system's [AccessibilityManager] as
     * returned from applicationContext.getSystemService
     * @returns services A list of ids of the enabled accessibility services. If
     *     the accessibility manager is disabled, returns null.
     */
    internal fun getEnabledAccessibilityServices(
        accessibilityManager: AccessibilityManager
    ): List<String>? {
        if (!accessibilityManager.isEnabled) {
            logger.info("AccessibilityManager is disabled")
            return null
        }
        return accessibilityManager.getEnabledAccessibilityServiceList(
            AccessibilityServiceInfo.FEEDBACK_ALL_MASK
        ).mapNotNull {
            // Note that any reference in java code can be null, so we'd better
            // check for null values here as well.
            it.id
        }
    }
}
