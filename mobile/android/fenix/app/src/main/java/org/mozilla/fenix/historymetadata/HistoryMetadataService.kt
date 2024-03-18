/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.historymetadata

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.launch
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.concept.storage.DocumentType
import mozilla.components.concept.storage.HistoryMetadataKey
import mozilla.components.concept.storage.HistoryMetadataObservation
import mozilla.components.concept.storage.HistoryMetadataStorage
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.base.utils.NamedThreadFactory
import java.util.concurrent.Executors

/**
 * Service for managing (creating, updating, deleting) history metadata.
 */
interface HistoryMetadataService {

    /**
     * Creates a history metadata record for the provided tab.
     *
     * @param tab the [TabSessionState] to record metadata for.
     * @param searchTerms Search terms associated with this metadata.
     * @param referrerUrl Referrer url associated with this metadata.
     */
    fun createMetadata(
        tab: TabSessionState,
        searchTerms: String? = null,
        referrerUrl: String? = null,
    ): HistoryMetadataKey

    /**
     * Updates the history metadata corresponding to the provided tab.
     *
     * @param key the [HistoryMetadataKey] identifying history metadata.
     * @param tab the [TabSessionState] to update history metadata for.
     */
    fun updateMetadata(key: HistoryMetadataKey, tab: TabSessionState)

    /**
     * Deletes history metadata records that haven't been updated since
     * the specified timestamp.
     *
     * @param olderThan timestamp indicating which records to delete.
     */
    fun cleanup(olderThan: Long)
}

class DefaultHistoryMetadataService(
    private val storage: HistoryMetadataStorage,
    private val scope: CoroutineScope = CoroutineScope(
        Executors.newSingleThreadExecutor(
            NamedThreadFactory("HistoryMetadataService"),
        ).asCoroutineDispatcher(),
    ),
) : HistoryMetadataService {

    private val logger = Logger("DefaultHistoryMetadataService")

    // NB: this map is only accessed from a single-thread executor (dispatcher of `scope`).
    private val tabsLastUpdated = mutableMapOf<String, Long>()

    override fun createMetadata(tab: TabSessionState, searchTerms: String?, referrerUrl: String?): HistoryMetadataKey {
        logger.debug("Creating metadata for tab ${tab.id}")

        val existingMetadata = tab.historyMetadata
        val metadataKey = if (existingMetadata != null && existingMetadata.url == tab.content.url) {
            existingMetadata
        } else {
            tab.toHistoryMetadataKey(searchTerms, referrerUrl)
        }

        val documentTypeObservation = HistoryMetadataObservation.DocumentTypeObservation(
            documentType = when (tab.mediaSessionState) {
                null -> DocumentType.Regular
                else -> DocumentType.Media
            },
        )

        scope.launch {
            storage.noteHistoryMetadataObservation(metadataKey, documentTypeObservation)
        }

        return metadataKey
    }

    override fun updateMetadata(key: HistoryMetadataKey, tab: TabSessionState) {
        val now = System.currentTimeMillis()
        val lastAccess = tab.lastAccess
        if (lastAccess == 0L) {
            logger.debug("Not updating metadata for tab $tab - lastAccess=0")
            return
        } else {
            logger.debug("Updating metadata for tab $tab")
        }

        // If it's possible that multiple threads overlap and run this block simultaneously, we
        // may over-observe, and record when we didn't intend to.
        // To make these cases easier to reason through (and likely correct),
        // `scope` is a single-threaded dispatcher. Execution of these blocks is thus serialized.
        scope.launch {
            val lastUpdated = tabsLastUpdated[tab.id] ?: 0
            if (lastUpdated > lastAccess) {
                logger.debug(
                    "Failed to update metadata because it was already recorded or lastAccess is incorrect",
                )
                return@launch
            }

            val viewTimeObservation = HistoryMetadataObservation.ViewTimeObservation(
                viewTime = (now - lastAccess).toInt(),
            )
            storage.noteHistoryMetadataObservation(key, viewTimeObservation)
            tabsLastUpdated[tab.id] = now
        }
    }

    override fun cleanup(olderThan: Long) {
        logger.debug("Deleting metadata last updated before $olderThan")

        scope.launch {
            storage.deleteHistoryMetadataOlderThan(olderThan)
        }
    }
}

fun TabSessionState.toHistoryMetadataKey(searchTerms: String?, referrerUrl: String?): HistoryMetadataKey =
    HistoryMetadataKey(
        url = content.url,
        referrerUrl = referrerUrl,
        searchTerm = searchTerms,
    )
