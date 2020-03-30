/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import androidx.lifecycle.ViewModel

class SharedViewModel : ViewModel() {

    /**
     * Used to remember if we need to scroll to the selected tab in the homeFragment's recycleView see #7356
     * */
    var shouldScrollToSelectedTab: Boolean = false
}
