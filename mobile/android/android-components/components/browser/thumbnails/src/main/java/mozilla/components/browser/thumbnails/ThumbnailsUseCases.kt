/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.thumbnails

import android.graphics.Bitmap
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.thumbnails.storage.ThumbnailStorage
import mozilla.components.support.base.log.logger.Logger

/**
 * Contains use cases related to the thumbnails feature.
 */
class ThumbnailsUseCases(
    store: BrowserStore,
    thumbnailStorage: ThumbnailStorage
) {
    /**
     * Load thumbnail use case.
     */
    class LoadThumbnailUseCase internal constructor(
        private val store: BrowserStore,
        private val thumbnailStorage: ThumbnailStorage
    ) {
        private val logger = Logger("ThumbnailsUseCases")

        /**
         * Loads the thumbnail of a tab from its in-memory [ContentState] or from the disk cache
         * of [ThumbnailStorage].
         */
        suspend operator fun invoke(sessionIdOrUrl: String): Bitmap? {
            val tab = store.state.findTab(sessionIdOrUrl)
            tab?.content?.thumbnail?.let {
                logger.debug(
                    "Loaded thumbnail from memory (sessionIdOrUrl = $sessionIdOrUrl, " +
                        "generationId = ${it.generationId})"
                )
                return@invoke it
            }

            return thumbnailStorage.loadThumbnail(sessionIdOrUrl).await()
        }
    }

    val loadThumbnail: LoadThumbnailUseCase by lazy {
        LoadThumbnailUseCase(
            store,
            thumbnailStorage
        )
    }
}
