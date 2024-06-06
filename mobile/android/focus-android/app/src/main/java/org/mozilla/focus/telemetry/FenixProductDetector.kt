/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.telemetry

import android.content.Context
import android.content.pm.ActivityInfo
import android.content.pm.PackageManager
import mozilla.components.support.utils.ext.getPackageInfoCompat

object FenixProductDetector {
    enum class FenixVersion(val packageName: String) {
        FIREFOX("org.mozilla.firefox"),
        FIREFOX_NIGHTLY("org.mozilla.fenix"),
        FIREFOX_BETA("org.mozilla.firefox_beta"),
    }

    fun getInstalledFenixVersions(context: Context): List<String> {
        val fenixVersions = mutableListOf<String>()

        for (product in FenixVersion.entries) {
            if (packageIsInstalled(context, product.packageName)) {
                fenixVersions.add(product.packageName)
            }
        }

        return fenixVersions
    }

    fun isFenixDefaultBrowser(defaultBrowser: ActivityInfo?): Boolean {
        if (defaultBrowser == null) return false

        for (product in FenixVersion.entries) {
            if (product.packageName == defaultBrowser.packageName) return true
        }
        return false
    }

    private fun packageIsInstalled(context: Context, packageName: String): Boolean {
        try {
            context.packageManager.getPackageInfoCompat(packageName, 0)
        } catch (e: PackageManager.NameNotFoundException) {
            return false
        }

        return true
    }
}
