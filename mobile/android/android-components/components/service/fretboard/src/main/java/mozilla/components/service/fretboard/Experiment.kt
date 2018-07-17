/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard

/**
 * Represents an A/B test experiment,
 * independent of the underlying
 * storage mechanism
 */
data class Experiment(
    val id: String,
    val name: String? = null,
    val description: String? = null,
    val match: Matcher? = null,
    val bucket: Bucket? = null,
    val lastModified: Long? = null,
    val payload: ExperimentPayload? = null
) {
    data class Matcher(
        val language: String? = null,
        val appId: String? = null,
        val regions: List<String>? = null,
        val version: String? = null,
        val manufacturer: String? = null,
        val device: String? = null,
        val country: String? = null
    )

    data class Bucket(
        val max: Int? = null,
        val min: Int? = null
    )

    override fun equals(other: Any?): Boolean {
        if (this === other) {
            return true
        }
        if (other == null || other !is Experiment) {
            return false
        }
        return other.id == id
    }

    override fun hashCode(): Int {
        return id.hashCode()
    }
}
