/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.GridItemSpan
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.foundation.lazy.grid.rememberLazyGridState
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.toMutableStateList
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.dimensionResource
import androidx.compose.ui.unit.dp
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.thumbnails.storage.ThumbnailStorage
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.tabstray.TabGridItem
import org.mozilla.fenix.compose.tabstray.TabListItem
import org.mozilla.fenix.tabstray.browser.compose.DragItemContainer
import org.mozilla.fenix.tabstray.browser.compose.createGridReorderState
import org.mozilla.fenix.tabstray.browser.compose.createListReorderState
import org.mozilla.fenix.tabstray.browser.compose.detectGridPressAndDragGestures
import org.mozilla.fenix.tabstray.browser.compose.detectVerticalPressAndDrag
import org.mozilla.fenix.tabstray.ext.MIN_COLUMN_WIDTH_DP
import org.mozilla.fenix.theme.FirefoxTheme
import kotlin.math.max

// Key for the span item at the bottom of the tray, used to make the item not reorderable.
const val SPAN_ITEM_KEY = "span"

// Key for the header item at the top of the tray, used to make the item not reorderable.
const val HEADER_ITEM_KEY = "header"

/**
 * Top-level UI for displaying a list of tabs.
 *
 * @param tabs The list of [TabSessionState] to display.
 * @param storage [ThumbnailStorage] to obtain tab thumbnail bitmaps from.
 * @param displayTabsInGrid Whether the tabs should be displayed in a grid.
 * @param selectedTabId The ID of the currently selected tab.
 * @param selectionMode [TabsTrayState.Mode] indicating whether the Tabs Tray is in single selection
 * or multi-selection and contains the set of selected tabs.
 * @param modifier
 * @param onTabClose Invoked when the user clicks to close a tab.
 * @param onTabMediaClick Invoked when the user interacts with a tab's media controls.
 * @param onTabClick Invoked when the user clicks on a tab.
 * @param onTabLongClick Invoked when the user long clicks a tab.
 * @param onMove Invoked when the user moves a tab.
 * @param onTabDragStart Invoked when starting to drag a tab.
 * @param header Optional layout to display before [tabs].
 */
@Suppress("LongParameterList")
@Composable
fun TabLayout(
    tabs: List<TabSessionState>,
    storage: ThumbnailStorage,
    displayTabsInGrid: Boolean,
    selectedTabId: String?,
    selectionMode: TabsTrayState.Mode,
    modifier: Modifier = Modifier,
    onTabClose: (TabSessionState) -> Unit,
    onTabMediaClick: (TabSessionState) -> Unit,
    onTabClick: (TabSessionState) -> Unit,
    onTabLongClick: (TabSessionState) -> Unit,
    onMove: (String, String?, Boolean) -> Unit,
    onTabDragStart: () -> Unit,
    header: (@Composable () -> Unit)? = null,
) {
    var selectedTabIndex = 0
    selectedTabId?.let {
        tabs.forEachIndexed { index, tab ->
            if (tab.id == selectedTabId) {
                selectedTabIndex = index
            }
        }
    }

    if (displayTabsInGrid) {
        TabGrid(
            tabs = tabs,
            storage = storage,
            selectedTabId = selectedTabId,
            selectedTabIndex = selectedTabIndex,
            selectionMode = selectionMode,
            modifier = modifier,
            onTabClose = onTabClose,
            onTabMediaClick = onTabMediaClick,
            onTabClick = onTabClick,
            onTabLongClick = onTabLongClick,
            onMove = onMove,
            onTabDragStart = onTabDragStart,
            header = header,
        )
    } else {
        TabList(
            tabs = tabs,
            storage = storage,
            selectedTabId = selectedTabId,
            selectedTabIndex = selectedTabIndex,
            selectionMode = selectionMode,
            modifier = modifier,
            onTabClose = onTabClose,
            onTabMediaClick = onTabMediaClick,
            onTabClick = onTabClick,
            onTabLongClick = onTabLongClick,
            onMove = onMove,
            onTabDragStart = onTabDragStart,
            header = header,
        )
    }
}

@OptIn(ExperimentalFoundationApi::class)
@Suppress("LongParameterList")
@Composable
private fun TabGrid(
    tabs: List<TabSessionState>,
    storage: ThumbnailStorage,
    selectedTabId: String?,
    selectedTabIndex: Int,
    selectionMode: TabsTrayState.Mode,
    modifier: Modifier = Modifier,
    onTabClose: (TabSessionState) -> Unit,
    onTabMediaClick: (TabSessionState) -> Unit,
    onTabClick: (TabSessionState) -> Unit,
    onTabLongClick: (TabSessionState) -> Unit,
    onMove: (String, String?, Boolean) -> Unit,
    onTabDragStart: () -> Unit,
    header: (@Composable () -> Unit)? = null,
) {
    val state = rememberLazyGridState(initialFirstVisibleItemIndex = selectedTabIndex)
    val tabListBottomPadding = dimensionResource(id = R.dimen.tab_tray_list_bottom_padding)
    val tabThumbnailSize = max(
        LocalContext.current.resources.getDimensionPixelSize(R.dimen.tab_tray_grid_item_thumbnail_height),
        LocalContext.current.resources.getDimensionPixelSize(R.dimen.tab_tray_grid_item_thumbnail_width),
    )
    val isInMultiSelectMode = selectionMode is TabsTrayState.Mode.Select

    val reorderState = createGridReorderState(
        gridState = state,
        onMove = { initialTab, newTab ->
            onMove(
                (initialTab.key as String),
                (newTab.key as String),
                initialTab.index < newTab.index,
            )
        },
        onLongPress = { itemInfo ->
            tabs.firstOrNull { tab -> tab.id == itemInfo.key }?.let { tab ->
                onTabLongClick(tab)
            }
        },
        onExitLongPress = onTabDragStart,
        ignoredItems = listOf(HEADER_ITEM_KEY, SPAN_ITEM_KEY),
    )
    var shouldLongPress by remember { mutableStateOf(!isInMultiSelectMode) }
    LaunchedEffect(selectionMode, reorderState.draggingItemKey) {
        if (reorderState.draggingItemKey == null) {
            shouldLongPress = selectionMode == TabsTrayState.Mode.Normal
        }
    }

    LazyVerticalGrid(
        columns = GridCells.Adaptive(minSize = MIN_COLUMN_WIDTH_DP.dp),
        modifier = modifier
            .fillMaxSize()
            .detectGridPressAndDragGestures(
                gridState = state,
                reorderState = reorderState,
                shouldLongPressToDrag = shouldLongPress,
            ),
        state = state,
    ) {
        header?.let {
            item(key = HEADER_ITEM_KEY, span = { GridItemSpan(maxLineSpan) }) {
                header()
            }
        }

        items(
            items = tabs,
            key = { tab -> tab.id },
        ) { tab ->
            DragItemContainer(state = reorderState, key = tab.id) {
                TabGridItem(
                    tab = tab,
                    thumbnailSize = tabThumbnailSize,
                    storage = storage,
                    isSelected = tab.id == selectedTabId,
                    multiSelectionEnabled = isInMultiSelectMode,
                    multiSelectionSelected = selectionMode.selectedTabs.contains(tab),
                    shouldClickListen = reorderState.draggingItemKey != tab.id,
                    onCloseClick = onTabClose,
                    onMediaClick = onTabMediaClick,
                    onClick = onTabClick,
                )
            }
        }

        item(key = SPAN_ITEM_KEY, span = { GridItemSpan(maxLineSpan) }) {
            Spacer(modifier = Modifier.height(tabListBottomPadding))
        }
    }
}

@OptIn(ExperimentalFoundationApi::class)
@Suppress("LongParameterList")
@Composable
private fun TabList(
    tabs: List<TabSessionState>,
    storage: ThumbnailStorage,
    selectedTabId: String?,
    selectedTabIndex: Int,
    selectionMode: TabsTrayState.Mode,
    modifier: Modifier = Modifier,
    onTabClose: (TabSessionState) -> Unit,
    onTabMediaClick: (TabSessionState) -> Unit,
    onTabClick: (TabSessionState) -> Unit,
    onTabLongClick: (TabSessionState) -> Unit,
    onMove: (String, String?, Boolean) -> Unit,
    header: (@Composable () -> Unit)? = null,
    onTabDragStart: () -> Unit = {},
) {
    val state = rememberLazyListState(initialFirstVisibleItemIndex = selectedTabIndex)
    val tabListBottomPadding = dimensionResource(id = R.dimen.tab_tray_list_bottom_padding)
    val tabThumbnailSize = max(
        LocalContext.current.resources.getDimensionPixelSize(R.dimen.tab_tray_list_item_thumbnail_height),
        LocalContext.current.resources.getDimensionPixelSize(R.dimen.tab_tray_list_item_thumbnail_width),
    )
    val isInMultiSelectMode = selectionMode is TabsTrayState.Mode.Select
    val reorderState = createListReorderState(
        listState = state,
        onMove = { initialTab, newTab ->
            onMove(
                (initialTab.key as String),
                (newTab.key as String),
                initialTab.index < newTab.index,
            )
        },
        onLongPress = {
            tabs.firstOrNull { tab -> tab.id == it.key }?.let { tab ->
                onTabLongClick(tab)
            }
        },
        onExitLongPress = onTabDragStart,
        ignoredItems = listOf(HEADER_ITEM_KEY, SPAN_ITEM_KEY),
    )
    var shouldLongPress by remember { mutableStateOf(!isInMultiSelectMode) }
    LaunchedEffect(selectionMode, reorderState.draggingItemKey) {
        if (reorderState.draggingItemKey == null) {
            shouldLongPress = selectionMode == TabsTrayState.Mode.Normal
        }
    }

    LazyColumn(
        modifier = modifier
            .fillMaxSize()
            .detectVerticalPressAndDrag(
                listState = state,
                reorderState = reorderState,
                shouldLongPressToDrag = shouldLongPress,
            ),
        state = state,
    ) {
        header?.let {
            item(key = HEADER_ITEM_KEY) {
                header()
            }
        }

        items(
            items = tabs,
            key = { tab -> tab.id },
        ) { tab ->
            DragItemContainer(state = reorderState, key = tab.id) {
                TabListItem(
                    tab = tab,
                    thumbnailSize = tabThumbnailSize,
                    storage = storage,
                    isSelected = tab.id == selectedTabId,
                    multiSelectionEnabled = isInMultiSelectMode,
                    multiSelectionSelected = selectionMode.selectedTabs.contains(tab),
                    shouldClickListen = reorderState.draggingItemKey != tab.id,
                    onCloseClick = onTabClose,
                    onMediaClick = onTabMediaClick,
                    onClick = onTabClick,
                )
            }
        }

        item(key = SPAN_ITEM_KEY) {
            Spacer(modifier = Modifier.height(tabListBottomPadding))
        }
    }
}

@LightDarkPreview
@Composable
private fun TabListPreview() {
    val tabs = remember { generateFakeTabsList().toMutableStateList() }

    FirefoxTheme {
        Box(
            modifier = Modifier
                .fillMaxSize()
                .background(FirefoxTheme.colors.layer1),
        ) {
            TabLayout(
                tabs = tabs,
                storage = ThumbnailStorage(LocalContext.current),
                selectedTabId = tabs[1].id,
                selectionMode = TabsTrayState.Mode.Normal,
                displayTabsInGrid = false,
                onTabClose = tabs::remove,
                onTabMediaClick = {},
                onTabClick = {},
                onTabLongClick = {},
                onTabDragStart = {},
                onMove = { _, _, _ -> },
            )
        }
    }
}

@LightDarkPreview
@Composable
private fun TabGridPreview() {
    val tabs = remember { generateFakeTabsList().toMutableStateList() }

    FirefoxTheme {
        Box(
            modifier = Modifier
                .fillMaxSize()
                .background(FirefoxTheme.colors.layer1),
        ) {
            TabLayout(
                tabs = tabs,
                storage = ThumbnailStorage(LocalContext.current),
                selectedTabId = tabs[0].id,
                selectionMode = TabsTrayState.Mode.Normal,
                displayTabsInGrid = true,
                onTabClose = tabs::remove,
                onTabMediaClick = {},
                onTabClick = {},
                onTabLongClick = {},
                onTabDragStart = {},
                onMove = { _, _, _ -> },
            )
        }
    }
}

@Suppress("MagicNumber")
@LightDarkPreview
@Composable
private fun TabGridMultiSelectPreview() {
    val tabs = generateFakeTabsList()
    val selectedTabs = remember { tabs.take(4).toMutableStateList() }

    FirefoxTheme {
        Box(
            modifier = Modifier
                .fillMaxSize()
                .background(FirefoxTheme.colors.layer1),
        ) {
            TabLayout(
                tabs = tabs,
                storage = ThumbnailStorage(LocalContext.current),
                selectedTabId = tabs[0].id,
                selectionMode = TabsTrayState.Mode.Select(selectedTabs.toSet()),
                displayTabsInGrid = false,
                onTabClose = {},
                onTabMediaClick = {},
                onTabClick = { tab ->
                    if (selectedTabs.contains(tab)) {
                        selectedTabs.remove(tab)
                    } else {
                        selectedTabs.add(tab)
                    }
                },
                onTabLongClick = {},
                onTabDragStart = {},
                onMove = { _, _, _ -> },
            )
        }
    }
}

private fun generateFakeTabsList(
    tabCount: Int = 10,
    isPrivate: Boolean = false,
): List<TabSessionState> =
    List(tabCount) { index ->
        TabSessionState(
            id = "tabId$index-$isPrivate",
            content = ContentState(
                url = "www.mozilla.com",
                private = isPrivate,
            ),
        )
    }
