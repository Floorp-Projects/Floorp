/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.ext

import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.selectedOrDefaultSearchEngine
import mozilla.components.browser.state.store.BrowserStore

/**
 * Returns the default search engine name or "custom" string if the engine is added by the user.
 */
fun BrowserStore.defaultSearchEngineName(): String {
    val defaultSearchEngine = state.search.selectedOrDefaultSearchEngine
    return if (defaultSearchEngine?.type == SearchEngine.Type.CUSTOM) {
        "custom"
    } else {
        defaultSearchEngine?.name ?: "<none>"
    }
}
