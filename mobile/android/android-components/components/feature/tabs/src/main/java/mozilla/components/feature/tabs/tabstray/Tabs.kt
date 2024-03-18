/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs.tabstray

import mozilla.components.browser.tabstray.TabsTray

/**
 * An internal only data class that is used for collecting the values to observe in the [TabsTray].
 * Aggregate data type keeping a reference to the list of tabs and the selected tab id.
 *
 * @property list The list of tabs.
 * @property selectedTabId Id of the selected tab in the list of tabs (or null).
 */
internal data class Tabs(
    val list: List<Tab>,
    val selectedTabId: String?,
)
