/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.content.Context
import androidx.annotation.VisibleForTesting
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.browser.state.state.content.DownloadState.Status
import mozilla.components.feature.downloads.AbstractFetchDownloadService

/**
 * Utilities for handling download dialogs.
 */
object DownloadDialogUtils {

    internal fun handleOnDownloadFinished(
        context: Context,
        downloadState: DownloadState,
        downloadJobStatus: Status,
        currentTab: SessionState?,
        onFinishedDialogShown: () -> Unit = {},
        onCannotOpenFile: (DownloadState) -> Unit,
    ) {
        // If the download is just paused, don't show any in-app notification
        if (shouldShowCompletedDownloadDialog(downloadState, downloadJobStatus, currentTab)) {
            if (downloadState.openInApp && downloadJobStatus == Status.COMPLETED) {
                val fileWasOpened = openFile(context, downloadState)
                if (!fileWasOpened) {
                    onCannotOpenFile(downloadState)
                }
            } else {
                onFinishedDialogShown()
            }
        }
    }

    @VisibleForTesting
    internal var openFile: (Context, DownloadState) -> (Boolean) = { context, downloadState ->
        AbstractFetchDownloadService.openFile(
            applicationContext = context.applicationContext,
            download = downloadState,
        )
    }

    /**
     * Indicates whether or not a completed download dialog should be shown.
     */
    fun shouldShowCompletedDownloadDialog(
        downloadState: DownloadState,
        status: Status,
        currentTab: SessionState?,
    ): Boolean {
        val isValidStatus =
            status in listOf(Status.COMPLETED, Status.FAILED)
        val isSameTab = downloadState.sessionId == (currentTab?.id ?: false)

        return isValidStatus && isSameTab
    }
}
