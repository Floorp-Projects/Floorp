/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser.ext

import androidx.fragment.app.Fragment
import org.mozilla.samples.browser.Components

/**
 * Get the components of this application.
 */
val Fragment.components: Components
    get() = context!!.components
