/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.storage

import android.os.Parcelable
import kotlinx.parcelize.Parcelize

/**
 * The possible document types to record history metadata for.
 */
enum class DocumentType {
    Regular,
    Media
}

/**
 * Represents the different types of history metadata observations.
 */
sealed class HistoryMetadataObservation {
    /**
     * A [HistoryMetadataObservation] to increment the total view time.
     */
    data class ViewTimeObservation(
        val viewTime: Int
    ) : HistoryMetadataObservation()

    /**
     * A [HistoryMetadataObservation] to update the document type.
     */
    data class DocumentTypeObservation(
        val documentType: DocumentType
    ) : HistoryMetadataObservation()
}
/**
 * Represents a set of history metadata values that uniquely identify a record. Note that
 * when recording observations, the same set of values may or may not cause a new record to be
 * created, depending on the de-bouncing logic of the underlying storage i.e. recording history
 * metadata observations with the exact same values may be combined into a single record.
 *
 * @property url A url of the page.
 * @property searchTerm An optional search term if this record was
 * created as part of a search by the user.
 * @property referrerUrl An optional url of the parent/referrer if
 * this record was created in response to a user opening
 * a page in a new tab.
 */
@Parcelize
data class HistoryMetadataKey(
    val url: String,
    val searchTerm: String? = null,
    val referrerUrl: String? = null
) : Parcelable

/**
 * Represents a history metadata record, which describes metadata for a history visit, such as metadata
 * about the page itself as well as metadata about how the page was opened.
 *
 * @property key The [HistoryMetadataKey] of this record.
 * @property title A title of the page.
 * @property createdAt When this metadata record was created.
 * @property updatedAt The last time this record was updated.
 * @property totalViewTime Total time the user viewed the page associated with this record.
 * @property documentType The [DocumentType] of the page.
 * @property previewImageUrl A preview image of the page (a.k.a. the hero image), if available.
 */
data class HistoryMetadata(
    val key: HistoryMetadataKey,
    val title: String?,
    val createdAt: Long,
    val updatedAt: Long,
    val totalViewTime: Int,
    val documentType: DocumentType,
    val previewImageUrl: String?
)

/**
 * An interface for interacting with a storage that manages [HistoryMetadata].
 */
interface HistoryMetadataStorage {
    /**
     * Returns the most recent [HistoryMetadata] for the provided [url].
     *
     * @param url Url to search by.
     * @return [HistoryMetadata] if there's a matching record, `null` otherwise.
     */
    suspend fun getLatestHistoryMetadataForUrl(url: String): HistoryMetadata?

    /**
     * Returns all [HistoryMetadata] where [HistoryMetadata.updatedAt] is greater or equal to [since].
     *
     * @param since Timestamp to search by.
     * @return A `List` of matching [HistoryMetadata], ordered by [HistoryMetadata.updatedAt] DESC.
     * Empty if nothing is found.
     */
    suspend fun getHistoryMetadataSince(since: Long): List<HistoryMetadata>

    /**
     * Returns all [HistoryMetadata] where [HistoryMetadata.updatedAt] is between [start] and [end], inclusive.
     *
     * @param start A `start` timestamp.
     * @param end An `end` timestamp.
     * @return A `List` of matching [HistoryMetadata], ordered by [HistoryMetadata.updatedAt] DESC.
     * Empty if nothing is found.
     */
    suspend fun getHistoryMetadataBetween(start: Long, end: Long): List<HistoryMetadata>

    /**
     * Searches through [HistoryMetadata] by [query], matching records by [HistoryMetadataKey.url],
     * [HistoryMetadata.title] and [HistoryMetadataKey.searchTerm].
     *
     * @param query A search query.
     * @param limit A maximum number of records to return.
     * @return A `List` of matching [HistoryMetadata], ordered by [HistoryMetadata.updatedAt] DESC.
     * Empty if nothing is found.
     */
    suspend fun queryHistoryMetadata(query: String, limit: Int): List<HistoryMetadata>

    /**
     * Records the provided [HistoryMetadataObservation] and updates the record identified by the
     * provided [HistoryMetadataKey].
     *
     * @param key the [HistoryMetadataKey] identifying the metadata records
     * @param observation the [HistoryMetadataObservation] to record.
     */
    suspend fun noteHistoryMetadataObservation(key: HistoryMetadataKey, observation: HistoryMetadataObservation)

    /**
     * Deletes [HistoryMetadata] with [HistoryMetadata.updatedAt] older than [olderThan].
     *
     * @param olderThan A timestamp to delete records by. Exclusive.
     */
    suspend fun deleteHistoryMetadataOlderThan(olderThan: Long)

    /**
     * Deletes metadata records that match [HistoryMetadataKey].
     */
    suspend fun deleteHistoryMetadata(key: HistoryMetadataKey)
}
