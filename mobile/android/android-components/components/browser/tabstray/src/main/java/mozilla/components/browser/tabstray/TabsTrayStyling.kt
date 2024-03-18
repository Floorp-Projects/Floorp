/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.tabstray

const val DEFAULT_ITEM_BACKGROUND_COLOR = 0xFFFFFFFF.toInt()
const val DEFAULT_ITEM_BACKGROUND_SELECTED_COLOR = 0xFFFF45A1FF.toInt()
const val DEFAULT_ITEM_TEXT_COLOR = 0xFF111111.toInt()
const val DEFAULT_ITEM_TEXT_SELECTED_COLOR = 0xFFFFFFFF.toInt()

/**
 * Tabs tray styling for items in the [TabsAdapter]. If a custom [TabViewHolder]
 * is used with [TabsAdapter.viewHolderProvider], the styling can be applied
 * when [TabViewHolder.bind] is invoked.
 *
 * @property itemBackgroundColor the background color for all non-selected tabs.
 * @property selectedItemBackgroundColor the background color for the selected tab.
 * @property itemTextColor the text color for all non-selected tabs.
 * @property selectedItemTextColor the text color for the selected tabs.
 * @property itemUrlTextColor the URL text color for all non-selected tabs.
 * @property selectedItemUrlTextColor the URL text color for the selected tab.
 */
data class TabsTrayStyling(
    val itemBackgroundColor: Int = DEFAULT_ITEM_BACKGROUND_COLOR,
    val selectedItemBackgroundColor: Int = DEFAULT_ITEM_BACKGROUND_SELECTED_COLOR,
    val itemTextColor: Int = DEFAULT_ITEM_TEXT_COLOR,
    val selectedItemTextColor: Int = DEFAULT_ITEM_TEXT_SELECTED_COLOR,
    val itemUrlTextColor: Int = DEFAULT_ITEM_TEXT_COLOR,
    val selectedItemUrlTextColor: Int = DEFAULT_ITEM_TEXT_SELECTED_COLOR,
)
