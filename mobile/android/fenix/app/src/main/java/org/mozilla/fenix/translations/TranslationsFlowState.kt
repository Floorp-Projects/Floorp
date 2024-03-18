/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations

import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState

/**
 * Convenience method to create a named pair for the translations flow.
 *
 * @property sessionState The session or tab state.
 * @property browserState The browser or global state.
 */
data class TranslationsFlowState(
    val sessionState: TabSessionState,
    val browserState: BrowserState,
)
