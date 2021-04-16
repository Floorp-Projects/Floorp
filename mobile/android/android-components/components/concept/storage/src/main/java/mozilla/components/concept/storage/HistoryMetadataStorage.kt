/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.storage

/**
 * Represents a history metadata record, which describes metadata for a history visit, such as metadata
 * about the page itself as well as metadata about how the page was opened.
 *
 * @property guid A global unique ID assigned to a saved record.
 * @property url A url of the page.
 * @property title A title of the page.
 * @property createdAt When this metadata record was created.
 * @property updatedAt The last time this record was updated.
 * @property totalViewTime Total time the user viewed the page associated with this record.
 * @property searchTerm An optional search term if this record was created as part of a search by the user.
 * @property isMedia A boolean describing if the page associated with this record is considered a media page.
 * @property parentUrl An optional parent url if this record was created in response to a user opening
 * a page in a new tab.
 */
data class HistoryMetadata(
    val guid: String? = null,
    val url: String,
    val title: String?,
    val createdAt: Long,
    val updatedAt: Long,
    val totalViewTime: Int,
    val searchTerm: String?,
    val isMedia: Boolean,
    val parentUrl: String?
)

/**
 * An interface for interacting with storage that manages [HistoryMetadata].
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
     * Searches through [HistoryMetadata] by [query], matching records by [HistoryMetadata.url],
     * [HistoryMetadata.title] and [HistoryMetadata.searchTerm].
     *
     * @param query A search query.
     * @param limit A maximum number of records to return.
     * @return A `List` of matching [HistoryMetadata], ordered by [HistoryMetadata.updatedAt] DESC.
     * Empty if nothing is found.
     */
    suspend fun queryHistoryMetadata(query: String, limit: Int): List<HistoryMetadata>

    /**
     * Adds a [HistoryMetadata] record to the storage.
     *
     * Note that if a corresponding history record already exists, we won't be updating
     * its title. The title update is expected to happen as part of a call to
     * [HistoryStorage.recordObservation]. Titles in [HistoryMetadata] and history are expected
     * to come from the same source (i.e. the page itself).
     *
     * @param metadata A [HistoryMetadata] record to add. Must not have a set [HistoryMetadata.guid].
     * @return A [HistoryMetadata.guid] of the added record.
     */
    suspend fun addHistoryMetadata(metadata: HistoryMetadata): String

    /**
     * Updates a [HistoryMetadata] based on [guid].
     *
     * @param totalViewTime A new [HistoryMetadata.totalViewTime].
     */
    suspend fun updateHistoryMetadata(guid: String, totalViewTime: Int)

    /**
     * Deletes [HistoryMetadata] with [HistoryMetadata.updatedAt] older than [olderThan].
     *
     * @param olderThan A timestamp to delete records by. Exclusive.
     */
    suspend fun deleteHistoryMetadataOlderThan(olderThan: Long)
}
