/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import mozilla.components.concept.tabstray.Tab
import java.util.UUID

/**
 * Helper for writing tests that need a [Tab].
 */
fun createTab(
    tabId: String = UUID.randomUUID().toString(),
    lastAccess: Long = 0L,
    searchTerm: String = ""
) = Tab(
    id = tabId,
    url = "https://mozilla.org",
    lastAccess = lastAccess,
    searchTerm = searchTerm
)
