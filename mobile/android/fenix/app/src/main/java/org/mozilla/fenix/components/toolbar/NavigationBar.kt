/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.Divider
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme

/**
 * Top-level UI for displaying the navigation bar.
 *
 * @param actionItems A list of [ActionItem] used to populate the bar.
 */
@Composable
fun NavigationBar(actionItems: List<ActionItem>) {
    Box(
        modifier = Modifier
            .background(FirefoxTheme.colors.layer1)
            .height(48.dp)
            .fillMaxWidth(),
    ) {
        Divider()

        Row(
            modifier = Modifier
                .align(Alignment.Center)
                .fillMaxWidth()
                .padding(horizontal = 16.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
        ) {
            actionItems.forEach {
                when (it.type) {
                    ItemType.STANDARD -> {
                        IconButton(onClick = {}) {
                            Icon(
                                painter = painterResource(it.iconId),
                                stringResource(id = it.descriptionResourceId),
                                tint = FirefoxTheme.colors.iconPrimary,
                            )
                        }
                    }
                    ItemType.TAB_COUNTER -> {
                        IconButton(onClick = {}) {
                            TabsIcon(item = it, tabsCount = 0)
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun TabsIcon(item: ActionItem, tabsCount: Int) {
    Box(contentAlignment = Alignment.Center) {
        Icon(
            painter = painterResource(id = item.iconId),
            contentDescription = stringResource(id = item.descriptionResourceId),
            tint = FirefoxTheme.colors.iconPrimary,
        )

        Text(
            text = tabsCount.toString(),
            color = FirefoxTheme.colors.iconPrimary,
            fontSize = 10.sp,
            fontWeight = FontWeight.W700,
            textAlign = TextAlign.Center,
        )
    }
}

/**
 * Represents a navigation bar element.
 *
 * @property iconId Resource ID of the icon that item should display.
 * @property descriptionResourceId Text used as a content description by accessibility services.
 * @property type Type of the item, defaults to [ItemType.STANDARD].
 */
data class ActionItem(
    val iconId: Int,
    val descriptionResourceId: Int,
    val type: ItemType = ItemType.STANDARD,
)

/**
 * Enumerates the types of items that can be used in a navigation bar.
 *
 * [STANDARD] - Represents a regular navigation item. Used for most navigation actions.
 * [TAB_COUNTER] - Represents a specialized item used to display a count, such as the number of open tabs in a browser.
 */
enum class ItemType {
    STANDARD, TAB_COUNTER
}

/**
 * Provides a collection of navigation items used in the application's navigation bar.
 */
object NavigationItems {
    val home = ActionItem(
        iconId = R.drawable.mozac_ic_home_24,
        descriptionResourceId = R.string.browser_toolbar_home,
    )

    val settings = ActionItem(
        iconId = R.drawable.mozac_ic_ellipsis_vertical_24,
        descriptionResourceId = R.string.mozac_browser_menu_button,
    )

    val back = ActionItem(
        iconId = R.drawable.mozac_ic_back_24,
        descriptionResourceId = R.string.browser_menu_back,
    )

    val forward = ActionItem(
        iconId = R.drawable.mozac_ic_forward_24,
        descriptionResourceId = R.string.browser_menu_forward,
    )

    val tabs = ActionItem(
        iconId = R.drawable.mozac_ui_tabcounter_box,
        descriptionResourceId = R.string.mozac_tab_counter_content_description,
        type = ItemType.TAB_COUNTER,
    )

    val defaultItems = listOf(back, forward, home, tabs, settings)
}

@LightDarkPreview
@Composable
private fun NavigationBarPreview() {
    FirefoxTheme {
        NavigationBar(NavigationItems.defaultItems)
    }
}

@Preview
@Composable
private fun NavigationBarPrivatePreview() {
    FirefoxTheme(theme = Theme.Private) {
        NavigationBar(NavigationItems.defaultItems)
    }
}
