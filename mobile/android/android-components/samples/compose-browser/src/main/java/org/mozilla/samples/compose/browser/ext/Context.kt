/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.compose.browser.ext

import android.content.Context
import org.mozilla.samples.compose.browser.BrowserApplication
import org.mozilla.samples.compose.browser.Components

val Context.application: BrowserApplication
    get() = applicationContext as BrowserApplication

val Context.components: Components
    get() = application.components
