/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.compose.browser

import android.app.Application

/**
 * The global [Application] class of this browser application.
 */
class BrowserApplication : Application() {
    val components by lazy { Components(this) }
}
