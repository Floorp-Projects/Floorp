/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.action

import mozilla.components.browser.session.store.BrowserStore
import mozilla.components.browser.session.state.BrowserState

/**
 * Generic interface for actions to be dispatched on a [BrowserStore].
 *
 * Actions are used to send data from the application to a [BrowserStore]. The [BrowserStore] will use the [Action] to
 * derive a new [BrowserState].
 */
interface Action
