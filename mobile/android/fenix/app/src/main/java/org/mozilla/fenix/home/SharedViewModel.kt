/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import androidx.lifecycle.ViewModel
import mozilla.components.browser.state.state.content.DownloadState

class SharedViewModel : ViewModel() {
    /**
     * Stores data needed for [DynamicDownloadDialog]. See #9044
     * Format: HashMap<sessionId, Pair<DownloadState, didFail>
     * */
    var downloadDialogState: HashMap<String?, Pair<DownloadState?, Boolean>> = HashMap()
}
