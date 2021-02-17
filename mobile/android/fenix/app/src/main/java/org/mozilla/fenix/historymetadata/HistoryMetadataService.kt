/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.historymetadata

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.concept.storage.DocumentType
import mozilla.components.concept.storage.HistoryMetadataKey
import mozilla.components.concept.storage.HistoryMetadataObservation
import mozilla.components.concept.storage.HistoryMetadataStorage
import mozilla.components.support.base.log.logger.Logger

/**
 * Service for managing (creating, updating, deleting) history metadata.
 */
interface HistoryMetadataService {

    /**
     * Creates a history metadata record for the provided tab.
     *
     * @param tab the [TabSessionState] to record metadata for.
     * @param parent the parent [TabSessionState] for search and domain grouping purposes.
     */
    fun createMetadata(tab: TabSessionState, parent: TabSessionState? = null): HistoryMetadataKey

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
    private val scope: CoroutineScope = CoroutineScope(Dispatchers.IO)
) : HistoryMetadataService {

    private val logger = Logger("DefaultHistoryMetadataService")

    override fun createMetadata(tab: TabSessionState, parent: TabSessionState?): HistoryMetadataKey {
        logger.debug("Creating metadata for tab ${tab.id}")

        val existingMetadata = tab.historyMetadata
        val metadataKey = if (existingMetadata != null && existingMetadata.url == tab.content.url) {
            existingMetadata
        } else {
            tab.toHistoryMetadataKey(parent)
        }

        val documentTypeObservation = HistoryMetadataObservation.DocumentTypeObservation(
            documentType = when (tab.mediaSessionState) {
                null -> DocumentType.Regular
                else -> DocumentType.Media
            }
        )

        scope.launch {
            storage.noteHistoryMetadataObservation(metadataKey, documentTypeObservation)
        }

        return metadataKey
    }

    override fun updateMetadata(key: HistoryMetadataKey, tab: TabSessionState) {
        logger.debug("Updating metadata for tab $tab")

        scope.launch {
            val viewTimeObservation = HistoryMetadataObservation.ViewTimeObservation(
                viewTime = (System.currentTimeMillis() - tab.lastAccess).toInt()
            )
            storage.noteHistoryMetadataObservation(key, viewTimeObservation)
        }
    }

    override fun cleanup(olderThan: Long) {
        logger.debug("Deleting metadata last updated before $olderThan")

        scope.launch {
            storage.deleteHistoryMetadataOlderThan(olderThan)
        }
    }
}

fun TabSessionState.toHistoryMetadataKey(parent: TabSessionState? = null): HistoryMetadataKey =
    HistoryMetadataKey(
        url = content.url,
        referrerUrl = parent?.content?.url,
        searchTerm = parent?.content?.searchTerms.takeUnless { it.isNullOrEmpty() }
    )
