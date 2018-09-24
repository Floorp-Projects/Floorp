/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.ext

import android.content.Context
import org.mozilla.focus.Components
import org.mozilla.focus.FocusApplication

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
