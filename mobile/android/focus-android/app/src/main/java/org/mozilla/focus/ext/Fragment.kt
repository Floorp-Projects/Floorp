/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.ext

import android.support.v4.app.Fragment
import org.mozilla.focus.Components

/**
 * Get the components of this application or null if this Fragment is not attached to a Context.
 */
val Fragment.components: Components?
    get() = context?.components

/**
 * Get the components of this application.
 *
 * This method will throw an exception if this Fragment is not attached to a Context.
 */
val Fragment.requireComponents: Components
    get() = requireContext().components
