/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.ext

import mozilla.components.browser.state.selector.findTabOrCustomTabOrSelectedTab
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.store.BrowserStore

fun BrowserStore.isSearch(sessionId: String): Boolean {
    return contentState(sessionId).searchTerms.isNotEmpty()
}

fun BrowserStore.contentState(sessionId: String): ContentState {
    return state.findTabOrCustomTabOrSelectedTab(sessionId)!!.content
}
