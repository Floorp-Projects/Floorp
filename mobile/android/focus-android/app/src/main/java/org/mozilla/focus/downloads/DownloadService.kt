/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.downloads

import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.fetch.Client
import mozilla.components.feature.downloads.AbstractFetchDownloadService
import org.mozilla.focus.ext.components

class DownloadService : AbstractFetchDownloadService() {
    override val httpClient: Client by lazy { components.client }
    override val store: BrowserStore by lazy { components.store }
}
