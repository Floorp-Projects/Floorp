/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.os.Parcelable
import androidx.lifecycle.ViewModel

class HomeScreenViewModel : ViewModel() {
    /**
     * Used to delete a specific session once the home screen is resumed
     */
    var sessionToDelete: String? = null

    var layoutManagerState: Parcelable? = null

    /**
     * Used to remember if we need to scroll to top of the homeFragment's recycleView (top sites) see #8561
     * */
    var shouldScrollToTopSites: Boolean = true
}
