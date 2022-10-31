/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.utils

/**
 * Data class for engine versions using semantic versioning (major.minor.patch).
 *
 * @param major Major version number
 * @param minor Minor version number
 * @param patch Patch version number
 * @param metadata Additional and optional metadata appended to the version number, e.g. for a version number of
 * "68.0a1" [metadata] will contain "a1".
 */
data class EngineVersion(
    val major: Int,
    val minor: Int,
    val patch: Long,
    val metadata: String? = null,
) {
    operator fun compareTo(other: EngineVersion): Int {
        return when {
            major != other.major -> major - other.major
            minor != other.minor -> minor - other.minor
            patch != other.patch -> (patch - other.patch).toInt()
            metadata != other.metadata -> when {
                metadata == null -> -1
                other.metadata == null -> 1
                else -> metadata.compareTo(other.metadata)
            }
            else -> 0
        }
    }

    /**
     * Returns true if this version number equals or is higher than the provided [major], [minor], [patch] version
     * numbers.
     */
    fun isAtLeast(major: Int, minor: Int = 0, patch: Long = 0): Boolean {
        return when {
            this.major > major -> true
            this.major < major -> false
            this.minor > minor -> true
            this.minor < minor -> false
            this.patch >= patch -> true
            else -> false
        }
    }

    override fun toString(): String {
        return buildString {
            append(major)
            append(".")
            append(minor)
            append(".")
            append(patch)
            if (metadata != null) {
                append(metadata)
            }
        }
    }

    companion object {
        /**
         * Parses the given [version] string and returns an [EngineVersion]. Returns null if the [version] string could
         * not be parsed successfully.
         */
        @Suppress("MagicNumber", "ReturnCount")
        fun parse(version: String): EngineVersion? {
            val majorRegex = "([0-9]+)"
            val minorRegex = "\\.([0-9]+)"
            val patchRegex = "(?:\\.([0-9]+))?"
            val metadataRegex = "([^0-9].*)?"
            val regex = "$majorRegex$minorRegex$patchRegex$metadataRegex".toRegex()
            val result = regex.matchEntire(version) ?: return null

            val major = result.groups[1]?.value ?: return null
            val minor = result.groups[2]?.value ?: return null
            val patch = result.groups[3]?.value ?: "0"
            val metadata = result.groups[4]?.value

            return try {
                EngineVersion(
                    major.toInt(),
                    minor.toInt(),
                    patch.toLong(),
                    metadata,
                )
            } catch (e: NumberFormatException) {
                null
            }
        }
    }
}
