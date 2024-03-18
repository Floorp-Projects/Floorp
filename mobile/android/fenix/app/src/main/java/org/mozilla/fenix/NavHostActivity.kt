/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import androidx.appcompat.app.ActionBar

/**
 * Interface for the main activity in a single-activity architecture.
 * All fragments will be displayed inside this activity.
 */
interface NavHostActivity {

    /**
     * Returns the support action bar, inflating it if necessary.
     * Everyone should call this instead of supportActionBar.
     */
    fun getSupportActionBarAndInflateIfNecessary(): ActionBar
}
