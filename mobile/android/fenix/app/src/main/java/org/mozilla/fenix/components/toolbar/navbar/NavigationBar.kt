/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar.navbar

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.LocalContentColor
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.dimensionResource
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.core.content.ContextCompat
import mozilla.components.browser.menu.view.MenuButton
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.ext.observeAsState
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.compose.LongPressIconButton
import org.mozilla.fenix.compose.TabCounter
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.search.SearchDialogFragment
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme
import org.mozilla.fenix.theme.ThemeManager

/**
 * Top-level UI for displaying the navigation bar.
 *
 * @param isPrivateMode If browsing in [BrowsingMode.Private].
 * @param browserStore The [BrowserStore] instance used to observe tabs state.
 * @param menuButton A [MenuButton] to be used as an [AndroidView]. The view implementation
 * contains the builder for the menu, so for the time being we are not implementing it as a composable.
 * @param onBackButtonClick Invoked when the user clicks the back button in the nav bar.
 * @param onBackButtonLongPress Invoked when the user long-presses the back button in the nav bar.
 * @param onForwardButtonClick Invoked when the user clicks the forward button in the nav bar.
 * @param onForwardButtonLongPress Invoked when the user long-presses the forward button in the nav bar.
 * @param onHomeButtonClick Invoked when the user clicks the home button in the nav bar.
 * @param onTabsButtonClick Invoked when the user clicks the tabs button in the nav bar.
 */
@Suppress("LongParameterList")
@Composable
fun BrowserNavBar(
    isPrivateMode: Boolean,
    browserStore: BrowserStore,
    menuButton: MenuButton,
    onBackButtonClick: () -> Unit,
    onBackButtonLongPress: () -> Unit,
    onForwardButtonClick: () -> Unit,
    onForwardButtonLongPress: () -> Unit,
    onHomeButtonClick: () -> Unit,
    onTabsButtonClick: () -> Unit,
) {
    val tabCount = browserStore.observeAsState(initialValue = 0) { browserState ->
        if (isPrivateMode) {
            browserState.privateTabs.size
        } else {
            browserState.normalTabs.size
        }
    }.value
    val canGoBack by browserStore.observeAsState(initialValue = false) { it.selectedTab?.content?.canGoBack ?: false }
    val canGoForward by browserStore.observeAsState(initialValue = false) {
        it.selectedTab?.content?.canGoForward ?: false
    }

    NavBar {
        BackButton(
            onBackButtonClick = onBackButtonClick,
            onBackButtonLongPress = onBackButtonLongPress,
            enabled = canGoBack,
        )

        ForwardButton(
            onForwardButtonClick = onForwardButtonClick,
            onForwardButtonLongPress = onForwardButtonLongPress,
            enabled = canGoForward,
        )

        HomeButton(
            onHomeButtonClick = onHomeButtonClick,
        )

        TabsButton(
            onTabsButtonClick = onTabsButtonClick,
            tabCount = tabCount,
        )

        MenuButton(menuButton = menuButton)
    }
}

/**
 * Top-level UI for displaying the navigation bar.
 *
 * @param isPrivateMode If browsing in [BrowsingMode.Private].
 * @param browserStore The [BrowserStore] instance used to observe tabs state.
 * @param menuButton A [MenuButton] to be used as an [AndroidView]. The view implementation
 * contains the builder for the menu, so for the time being we are not implementing it as a composable.
 * @param onSearchButtonClick Invoked when the user clicks the search button in the nav bar. The button
 * is visible only on home screen and activates [SearchDialogFragment].
 * @param onTabsButtonClick Invoked when the user clicks the tabs button in the nav bar.
 */
@Composable
fun HomeNavBar(
    isPrivateMode: Boolean,
    browserStore: BrowserStore,
    menuButton: MenuButton,
    onSearchButtonClick: () -> Unit,
    onTabsButtonClick: () -> Unit,
) {
    val tabCount = browserStore.observeAsState(initialValue = 0) { browserState ->
        if (isPrivateMode) {
            browserState.privateTabs.size
        } else {
            browserState.normalTabs.size
        }
    }.value

    NavBar {
        BackButton(
            onBackButtonClick = {
                // no-op
            },
            onBackButtonLongPress = {
                // no-op
            },
            // Nav buttons are disabled on the home screen
            enabled = false,
        )

        ForwardButton(
            onForwardButtonClick = {
                // no-op
            },
            onForwardButtonLongPress = {
                // no-op
            },
            // Nav buttons are disabled on the home screen
            enabled = false,
        )

        SearchWebButton(
            onSearchButtonClick = onSearchButtonClick,
        )

        TabsButton(
            onTabsButtonClick = onTabsButtonClick,
            tabCount = tabCount,
        )

        MenuButton(menuButton = menuButton)
    }
}

@Composable
private fun NavBar(
    content: @Composable RowScope.() -> Unit,
) {
    Row(
        modifier = Modifier
            .background(FirefoxTheme.colors.layer1)
            .height(dimensionResource(id = R.dimen.browser_navbar_height))
            .fillMaxWidth()
            .padding(horizontal = 16.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        content = content,
    )
}

@Composable
private fun BackButton(
    onBackButtonClick: () -> Unit,
    onBackButtonLongPress: () -> Unit,
    enabled: Boolean,
) {
    LongPressIconButton(
        onClick = onBackButtonClick,
        onLongClick = onBackButtonLongPress,
        enabled = enabled,
    ) {
        Icon(
            painter = painterResource(R.drawable.mozac_ic_back_24),
            stringResource(id = R.string.browser_menu_back),
            tint = if (enabled) FirefoxTheme.colors.iconPrimary else FirefoxTheme.colors.iconDisabled,
        )
    }
}

@Composable
private fun ForwardButton(
    onForwardButtonClick: () -> Unit,
    onForwardButtonLongPress: () -> Unit,
    enabled: Boolean,
) {
    LongPressIconButton(
        onClick = onForwardButtonClick,
        onLongClick = onForwardButtonLongPress,
        enabled = enabled,
    ) {
        Icon(
            painter = painterResource(R.drawable.mozac_ic_forward_24),
            stringResource(id = R.string.browser_menu_forward),
            tint = if (enabled) FirefoxTheme.colors.iconPrimary else FirefoxTheme.colors.iconDisabled,
        )
    }
}

@Composable
private fun HomeButton(
    onHomeButtonClick: () -> Unit,
) {
    IconButton(
        onClick = onHomeButtonClick,
    ) {
        Icon(
            painter = painterResource(R.drawable.mozac_ic_home_24),
            stringResource(id = R.string.browser_toolbar_home),
            tint = FirefoxTheme.colors.iconPrimary,
        )
    }
}

@Composable
private fun SearchWebButton(
    onSearchButtonClick: () -> Unit,
) {
    IconButton(
        onClick = onSearchButtonClick,
    ) {
        Icon(
            painter = painterResource(R.drawable.mozac_ic_search_24),
            stringResource(id = R.string.search_hint),
            tint = FirefoxTheme.colors.iconPrimary,
        )
    }
}

@Composable
private fun MenuButton(
    menuButton: MenuButton,
) {
    // Should refactor it to be a simple IconButton with a click listener
    // once the redesigned menu is implemented.
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1884049
    AndroidView(
        modifier = Modifier.size(48.dp),
        factory = { _ -> menuButton },
    )
}

@Composable
private fun TabsButton(
    onTabsButtonClick: () -> Unit,
    tabCount: Int,
) {
    CompositionLocalProvider(LocalContentColor provides FirefoxTheme.colors.iconPrimary) {
        IconButton(onClick = { onTabsButtonClick() }) {
            TabCounter(tabCount = tabCount)
        }
    }
}

@Composable
private fun HomeNavBarPreviewRoot(isPrivateMode: Boolean) {
    val context = LocalContext.current
    val colorId = if (isPrivateMode) {
        // private mode preview keeps using black colour as textPrimary
        ThemeManager.resolveAttribute(R.attr.textOnColorPrimary, context)
    } else {
        ThemeManager.resolveAttribute(R.attr.textPrimary, context)
    }
    val menuButton = MenuButton(context).apply {
        setColorFilter(
            ContextCompat.getColor(
                context,
                colorId,
            ),
        )
    }

    HomeNavBar(
        onSearchButtonClick = {},
        menuButton = menuButton,
        onTabsButtonClick = {},
        isPrivateMode = false,
        browserStore = BrowserStore(),
    )
}

@Composable
private fun OpenTabNavBarNavBarPreviewRoot(isPrivateMode: Boolean) {
    val context = LocalContext.current
    val colorId = if (isPrivateMode) {
        // private mode preview keeps using black colour as textPrimary
        ThemeManager.resolveAttribute(R.attr.textOnColorPrimary, context)
    } else {
        ThemeManager.resolveAttribute(R.attr.textPrimary, context)
    }
    val menuButton = MenuButton(context).apply {
        setColorFilter(
            ContextCompat.getColor(
                context,
                colorId,
            ),
        )
    }

    BrowserNavBar(
        onBackButtonClick = {},
        onBackButtonLongPress = {},
        onForwardButtonClick = {},
        onForwardButtonLongPress = {},
        onHomeButtonClick = {},
        menuButton = menuButton,
        onTabsButtonClick = {},
        isPrivateMode = false,
        browserStore = BrowserStore(),
    )
}

@LightDarkPreview
@Composable
private fun HomeNavBarPreview() {
    FirefoxTheme {
        HomeNavBarPreviewRoot(isPrivateMode = false)
    }
}

@Preview
@Composable
private fun HomeNavBarPrivatePreview() {
    FirefoxTheme(theme = Theme.Private) {
        HomeNavBarPreviewRoot(isPrivateMode = true)
    }
}

@LightDarkPreview
@Composable
private fun OpenTabNavBarPreview() {
    FirefoxTheme {
        OpenTabNavBarNavBarPreviewRoot(isPrivateMode = false)
    }
}

@Preview
@Composable
private fun OpenTabNavBarPrivatePreview() {
    FirefoxTheme(theme = Theme.Private) {
        OpenTabNavBarNavBarPreviewRoot(isPrivateMode = true)
    }
}
