/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.browser.state.state.content.DownloadState.Status
import mozilla.components.feature.downloads.AbstractFetchDownloadService
import org.mozilla.fenix.R
import org.mozilla.fenix.downloads.DynamicDownloadDialog

internal fun BaseBrowserFragment.handleOnDownloadFinished(
    downloadState: DownloadState,
    downloadJobStatus: Status,
    tryAgain: (String) -> Unit,
) {
    // If the download is just paused, don't show any in-app notification
    if (shouldShowCompletedDownloadDialog(downloadState, downloadJobStatus)) {
        val safeContext = context ?: return
        val onCannotOpenFile: (DownloadState) -> Unit = {
            showCannotOpenFileError(binding.dynamicSnackbarContainer, safeContext, it)
        }
        if (downloadState.openInApp && downloadJobStatus == Status.COMPLETED) {
            val fileWasOpened = AbstractFetchDownloadService.openFile(
                applicationContext = safeContext.applicationContext,
                download = downloadState,
            )
            if (!fileWasOpened) {
                onCannotOpenFile(downloadState)
            }
        } else {
            saveDownloadDialogState(
                downloadState.sessionId,
                downloadState,
                downloadJobStatus,
            )

            val dynamicDownloadDialog = DynamicDownloadDialog(
                context = safeContext,
                downloadState = downloadState,
                didFail = downloadJobStatus == Status.FAILED,
                tryAgain = tryAgain,
                onCannotOpenFile = onCannotOpenFile,
                binding = binding.viewDynamicDownloadDialog,
                toolbarHeight = resources.getDimensionPixelSize(R.dimen.browser_toolbar_height),
            ) { sharedViewModel.downloadDialogState.remove(downloadState.sessionId) }

            dynamicDownloadDialog.show()
            browserToolbarView.expand()
        }
    }
}
