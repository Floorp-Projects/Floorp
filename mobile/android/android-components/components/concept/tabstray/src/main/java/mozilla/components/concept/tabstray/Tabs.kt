/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.tabstray

/**
 * Aggregate data type keeping a reference to the list of tabs and the index of the selected tab.
 *
 * @property list The list of tabs.
 * @property selectedTabId Id of the selected tab in the list of tabs (or null).
 */
@Deprecated(
    "This will be removed in future versions",
    ReplaceWith("TabList", "mozilla.components.feature.tabs.tabstray"),
)
@Suppress("Deprecation")
data class Tabs(
    val list: List<Tab>,
    val selectedTabId: String?,
)
