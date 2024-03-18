/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils

object StorageUtils {

    // Borrowed from https://gist.github.com/ademar111190/34d3de41308389a0d0d8
    fun levenshteinDistance(a: String, b: String): Int {
        val lhsLength = a.length
        val rhsLength = b.length

        // Levenshtein distance upper bound is at most the length of the longer string.
        // However, for our use case we want distance from an empty string to a non-empty string to
        // be arbitrarily high; otherwise, an empty string will be of varying distances from strings
        // of varying lengths. This is the correct result for Levenshtein distance, but an incorrect
        // outcome for our domain. In other words, Levenshtein distance isn't exactly what we need.
        if (lhsLength == 0 || rhsLength == 0) {
            return Int.MAX_VALUE
        }

        var cost = Array(lhsLength) { it }
        var newCost = Array(lhsLength) { 0 }

        for (i in 1 until rhsLength) {
            newCost[0] = i

            for (j in 1 until lhsLength) {
                val match = if (a[j - 1] == b[i - 1]) 0 else 1

                val costReplace = cost[j - 1] + match
                val costInsert = cost[j] + 1
                val costDelete = newCost[j - 1] + 1

                newCost[j] = Math.min(Math.min(costInsert, costDelete), costReplace)
            }

            val swap = cost
            cost = newCost
            newCost = swap
        }

        return cost[lhsLength - 1]
    }
}
