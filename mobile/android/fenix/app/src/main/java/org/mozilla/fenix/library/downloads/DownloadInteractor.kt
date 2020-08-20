/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.downloads
/**
 * Interactor for the download screen
 * Provides implementations for the DownloadViewInteractor
 */
@SuppressWarnings("TooManyFunctions")
class DownloadInteractor(
    private val downloadController: DownloadController
) : DownloadViewInteractor {
    override fun open(item: DownloadItem) {
        downloadController.handleOpen(item)
    }

    override fun select(item: DownloadItem) { /* noop */ }

    override fun deselect(item: DownloadItem) { /* noop */ }

    override fun onBackPressed(): Boolean {
        return downloadController.handleBackPressed()
    }
}
