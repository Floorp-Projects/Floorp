/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.ext

import android.app.Activity
import android.content.Context
import android.view.ContextThemeWrapper
import android.view.accessibility.AccessibilityManager
import mozilla.components.support.utils.ext.getPackageInfoCompat
import org.mozilla.focus.Components
import org.mozilla.focus.FocusApplication
import org.mozilla.focus.utils.Settings
import org.mozilla.gecko.util.HardwareUtils
import java.text.DateFormat

/**
 * Get the FocusApplication object from a context.
 */
val Context.application: FocusApplication
    get() = applicationContext as FocusApplication

/**
 * Get the components of this application.
 */
val Context.components: Components
    get() = application.components

/**
 * Get the settings of this application.
 */
val Context.settings: Settings
    get() = application.components.settings

/**
 * System's [AccessibilityManager].
 */
val Context.accessibilityManager: AccessibilityManager
    get() = getSystemService(Context.ACCESSIBILITY_SERVICE) as AccessibilityManager

/**
 * Get the app install date.
 */
val Context.installedDate: String
    get() {
        val installTime = this.packageManager.getPackageInfoCompat(this.packageName, 0).firstInstallTime
        return DateFormat.getDateInstance().format(installTime)
    }

/**
 * Checks if the current device is a tablet.
 */
fun Context.isTablet(): Boolean = HardwareUtils.isTablet(this)

/**
 * Casts [Context] to [Activity].
 */
fun Context.tryAsActivity() =
    (this as? ContextThemeWrapper)?.baseContext as? Activity ?: this as? Activity
