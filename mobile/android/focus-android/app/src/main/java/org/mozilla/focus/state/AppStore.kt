/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.state

import mozilla.components.lib.state.Store

/**
 * A [Store] keeping track of the global app state.
 */
class AppStore(
    initialState: AppState,
) : Store<AppState, AppAction>(initialState, AppReducer)
