/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.downloads

import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.background
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.hapticfeedback.HapticFeedbackType
import androidx.compose.ui.platform.LocalHapticFeedback
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.PreviewLightDark
import androidx.compose.ui.tooling.preview.PreviewParameter
import androidx.compose.ui.tooling.preview.PreviewParameterProvider
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.lib.state.ext.observeAsState
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.list.SelectableListItem
import org.mozilla.fenix.ext.getIcon
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Downloads screen that displays the list of downloads.
 *
 * @param downloadsStore The [DownloadFragmentStore] used to manage and access the state of download items.
 * @param onItemClick Invoked when a download item is clicked.
 * @param onItemDeleteClick Invoked when delete icon button is clicked.
 */
@Composable
fun DownloadsScreen(
    downloadsStore: DownloadFragmentStore,
    onItemClick: (DownloadItem) -> Unit,
    onItemDeleteClick: (DownloadItem) -> Unit,
) {
    val uiState by downloadsStore.observeAsState(initialValue = downloadsStore.state) { it }

    if (uiState.isEmptyState) {
        NoDownloads(
            modifier = Modifier
                .fillMaxSize()
                .background(FirefoxTheme.colors.layer1),
        )
    } else {
        DownloadsContent(
            state = uiState,
            onClick = onItemClick,
            onSelectionChange = { item, isSelected ->
                if (isSelected) {
                    downloadsStore.dispatch(DownloadFragmentAction.AddItemForRemoval(item))
                } else {
                    downloadsStore.dispatch(DownloadFragmentAction.RemoveItemForRemoval(item))
                }
            },
            onDeleteClick = onItemDeleteClick,
            modifier = Modifier
                .fillMaxSize()
                .background(FirefoxTheme.colors.layer1),
        )
    }
}

@Composable
@OptIn(ExperimentalFoundationApi::class)
private fun DownloadsContent(
    state: DownloadFragmentState,
    onClick: (DownloadItem) -> Unit,
    onSelectionChange: (DownloadItem, Boolean) -> Unit,
    onDeleteClick: (DownloadItem) -> Unit,
    modifier: Modifier = Modifier,
) {
    val haptics = LocalHapticFeedback.current

    LazyColumn(
        modifier = modifier,
    ) {
        items(
            items = state.itemsToDisplay,
            key = { it.id },
        ) { downloadItem ->
            SelectableListItem(
                label = downloadItem.fileName ?: downloadItem.url,
                description = downloadItem.formattedSize,
                isSelected = state.mode.selectedItems.contains(downloadItem),
                icon = downloadItem.getIcon(),
                afterListAction = {
                    if (state.isNormalMode) {
                        IconButton(
                            onClick = { onDeleteClick(downloadItem) },
                        ) {
                            Icon(
                                painter = painterResource(id = R.drawable.ic_delete),
                                contentDescription = stringResource(id = R.string.download_delete_item_1),
                                tint = FirefoxTheme.colors.iconPrimary,
                            )
                        }
                    }
                },
                modifier = Modifier
                    .animateItemPlacement()
                    .combinedClickable(
                        onClick = {
                            if (state.isNormalMode) {
                                onClick(downloadItem)
                            } else {
                                onSelectionChange(
                                    downloadItem,
                                    !state.mode.selectedItems.contains(downloadItem),
                                )
                            }
                        },
                        onLongClick = {
                            if (state.isNormalMode) {
                                haptics.performHapticFeedback(HapticFeedbackType.LongPress)
                                onSelectionChange(downloadItem, true)
                            }
                        },
                    )
                    .testTag("${DownloadsListTestTag.DOWNLOADS_LIST_ITEM}.${downloadItem.fileName}"),
            )
        }
    }
}

@Composable
private fun NoDownloads(modifier: Modifier = Modifier) {
    Box(
        modifier = modifier,
        contentAlignment = Alignment.Center,
    ) {
        Text(
            text = stringResource(id = R.string.download_empty_message_1),
            modifier = Modifier,
            color = FirefoxTheme.colors.textSecondary,
            style = FirefoxTheme.typography.body1,
        )
    }
}

private class DownloadsScreenPreviewModelParameterProvider :
    PreviewParameterProvider<DownloadFragmentState> {
    override val values: Sequence<DownloadFragmentState>
        get() = sequenceOf(
            DownloadFragmentState.INITIAL,
            DownloadFragmentState(
                items = listOf(
                    DownloadItem(
                        id = "1",
                        fileName = "File 1",
                        url = "https://example.com/file1",
                        formattedSize = "1.2 MB",
                        contentType = "application/pdf",
                        status = DownloadState.Status.COMPLETED,
                        filePath = "/path/to/file1",
                    ),
                    DownloadItem(
                        id = "2",
                        fileName = "File 2",
                        url = "https://example.com/file2",
                        formattedSize = "2.3 MB",
                        contentType = "image/png",
                        status = DownloadState.Status.COMPLETED,
                        filePath = "/path/to/file1",
                    ),
                    DownloadItem(
                        id = "3",
                        fileName = "File 3",
                        url = "https://example.com/file3",
                        formattedSize = "3.4 MB",
                        contentType = "application/zip",
                        status = DownloadState.Status.COMPLETED,
                        filePath = "/path/to/file1",
                    ),
                ),
                mode = DownloadFragmentState.Mode.Normal,
                pendingDeletionIds = emptySet(),
                isDeletingItems = false,
            ),
        )
}

@Composable
@PreviewLightDark
private fun DownloadsScreenPreviews(
    @PreviewParameter(DownloadsScreenPreviewModelParameterProvider::class) state: DownloadFragmentState,
) {
    FirefoxTheme {
        DownloadsScreen(
            downloadsStore = DownloadFragmentStore(
                initialState = state,
            ),
            onItemClick = {},
            onItemDeleteClick = {},
        )
    }
}
