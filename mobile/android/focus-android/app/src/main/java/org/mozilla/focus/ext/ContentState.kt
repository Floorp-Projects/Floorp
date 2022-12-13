/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.ext

import mozilla.components.browser.state.state.ContentState

val ContentState.isSearch: Boolean
    get() = searchTerms.isNotEmpty()

/**
 * Returns the tab site title or domain name if title is empty.
 */
val ContentState.titleOrDomain: String
    get() = title.ifEmpty { url.tryGetRootDomain }
