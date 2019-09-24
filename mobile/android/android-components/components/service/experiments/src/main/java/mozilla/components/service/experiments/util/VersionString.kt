/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.experiments.util

import kotlin.math.max

internal class VersionString(
    val version: String
) : Comparable<VersionString> {
    companion object {
        // Validates that the version matches expected version formatting
        // This matches any number of digits separated by dots.
        // Valid examples:
        //  1.0.1
        //  1.0
        //  10.4.123545
        // Invalid examples:
        // a.b.c
        // Not.a.version
        // 12.1.1A
        val VERSION_REGEX = "[0-9]+(\\.[0-9]+)*".toRegex()
    }

    @Suppress("ComplexMethod")
    override fun compareTo(other: VersionString): Int {
        if (!version.matches(VERSION_REGEX)) {
            throw IllegalArgumentException("Unexpected format in VersionString")
        }
        if (!other.version.matches(VERSION_REGEX)) {
            throw IllegalArgumentException("Unexpected format in VersionString")
        }
        val thisParts = version.split(".")
        val otherParts = other.version.split(".")
        val length = max(thisParts.count(), otherParts.count())
        for (i in 0..length) {
            // Get the current part, or zero if there isn't a part to compare to for thisPart
            val thisPart = if (i < thisParts.count()) {
                thisParts[i].toInt()
            } else {
                0
            }
            // Get the current part, or zero if there isn't a part to compare to for otherPart
            val otherPart = if (i < otherParts.count()) {
                otherParts[i].toInt()
            } else {
                0
            }

            if (thisPart < otherPart) {
                return -1
            }
            if (thisPart > otherPart) {
                return 1
            }
        }
        return 0
    }

    override fun equals(other: Any?): Boolean {
        if (other == null) {
            return false
        }
        if (this.hashCode() == other.hashCode()) {
            return true
        }
        if (this.javaClass != other.javaClass) {
            return false
        }

        return this.compareTo(other as VersionString) == 0
    }

    override fun hashCode(): Int {
        return version.hashCode()
    }
}
