/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.data

import java.util.Locale

/**
 * The metadata for a spoken article's audio file.
 *
 * Some documentation on these values, directly from the endpoint, can be found here:
 *   https://documenter.getpostman.com/view/777613/S17jXChA#426b37e2-0a4f-bd65-35c6-e14f1b19d0f0
 *
 * @property format the encoding format of the audio file, e.g. "mp3".
 * @property audioUrl the url to the spoken article's audio file.
 * @property status whether or not the audio file is available.
 * @property voice the voice name used to speak the article content, e.g. "Salli".
 * @property durationSeconds length of the audio in seconds.
 * @property size size of the audio file in bytes.
 */
// If we change the properties, e.g. adding a field, any consumers that rely on persisting and constructing new
// instances of this data type, will be required to implement code to migrate the persisted data. To prevent this,
// we mark the constructors internal. If a consumer is required to persist this data, we should consider providing a
// persistence solution.
data class PocketListenArticleMetadata internal constructor(
    val format: String,
    val audioUrl: String,
    val status: Status,
    val voice: String,
    val durationSeconds: Long,
    val size: String
) {

    /**
     * A status representing whether or not the audio file is available.
     */
    enum class Status {
        /** The audio file is available: this should always be returned for mp3 files. */
        AVAILABLE,

        /** The audio file is processing (e.g. transcoding).  */
        PROCESSING,
        UNKNOWN;

        /** The string we converted from when [fromString] is called: used for debugging. */
        internal var originalString: String? = null

        companion object {
            internal fun fromString(string: String): Status = when (string.toLowerCase(Locale.ENGLISH)) {
                "available" -> AVAILABLE
                "processing" -> PROCESSING
                else -> UNKNOWN
            }.also { it.originalString = string }
        }
    }
}
