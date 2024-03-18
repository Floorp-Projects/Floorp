/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser.ext

import android.content.Context
import org.mozilla.samples.browser.Components
import org.mozilla.samples.browser.SampleApplication

/**
 * Get the SampleApplication object from a context.
 */
val Context.application: SampleApplication
    get() = applicationContext as SampleApplication

/**
 * Get the components of this application.
 */
val Context.components: Components
    get() = application.components
