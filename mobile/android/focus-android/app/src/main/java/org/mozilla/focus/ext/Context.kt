/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.ext

import android.content.Context
import org.mozilla.focus.Components
import org.mozilla.focus.FocusApplication
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
 * Get the app install date.
 */
val Context.installedDate: String
    get() {
        val installTime = this.packageManager.getPackageInfo(this.packageName, 0).firstInstallTime
        return DateFormat.getDateInstance().format(installTime)
    }

/**
 * Get the app last update date.
 */
val Context.lastUpdateDate: String
    get() {
        val lastUpdateDate = this.packageManager.getPackageInfo(this.packageName, 0).lastUpdateTime
        return DateFormat.getDateInstance().format(lastUpdateDate)
    }
