/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser.media

import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.media.service.AbstractMediaSessionService
import org.mozilla.samples.browser.ext.components

/**
 * See [AbstractMediaSessionService].
 */
class MediaSessionService : AbstractMediaSessionService() {
    override val store: BrowserStore by lazy { components.store }
}
