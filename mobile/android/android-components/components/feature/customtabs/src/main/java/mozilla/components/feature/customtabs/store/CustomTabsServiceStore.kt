/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.customtabs.store

import mozilla.components.lib.state.Store

class CustomTabsServiceStore(
    initialState: CustomTabsServiceState = CustomTabsServiceState(),
) : Store<CustomTabsServiceState, CustomTabsAction>(
    initialState,
    CustomTabsServiceStateReducer::reduce,
    threadNamePrefix = "CustomTabsService",
)
