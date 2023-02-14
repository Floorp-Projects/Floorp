/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

import android.content.Context
import android.os.Build
import android.provider.Settings
import androidx.annotation.RequiresApi
import java.io.File

/**
 * Provides access to system properties.
 */
interface BootUtils {

    /**
     * Gets the device boot count.
     *
     * **Only for Android versions N(24) and above.**
     */
    @RequiresApi(Build.VERSION_CODES.N)
    fun getDeviceBootCount(context: Context): String

    val deviceBootId: String?

    val bootIdFileExists: Boolean

    companion object {
        /**
         * @return either the boot count or a boot id depending on the device Android version.
         */
        fun getBootIdentifier(context: Context, bootUtils: BootUtils = BootUtilsImpl()): String {
            return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                bootUtils.getDeviceBootCount(context)
            } else {
                return if (bootUtils.bootIdFileExists) {
                    bootUtils.deviceBootId ?: NO_BOOT_IDENTIFIER
                } else {
                    NO_BOOT_IDENTIFIER
                }
            }
        }
    }
}

/**
 * Implementation of [BootUtils].
 */
class BootUtilsImpl : BootUtils {
    private val bootIdFile by lazy { File("/proc/sys/kernel/random/boot_id") }

    @RequiresApi(Build.VERSION_CODES.N)
    override fun getDeviceBootCount(context: Context): String =
        Settings.Global.getString(context.contentResolver, Settings.Global.BOOT_COUNT)

    override val deviceBootId: String? by lazy { bootIdFile.readLines().firstOrNull()?.trim() }

    override val bootIdFileExists: Boolean by lazy { bootIdFile.exists() }
}

private const val NO_BOOT_IDENTIFIER = "no boot identifier available"
