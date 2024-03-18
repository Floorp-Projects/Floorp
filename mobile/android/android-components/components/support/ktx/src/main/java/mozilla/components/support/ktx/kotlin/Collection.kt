/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.kotlin

/**
 * Performs a cartesian product of all the elements in two collections and returns each pair to
 * the [block] function.
 *
 * Example:
 *
 * ```kotlin
 * val numbers = listOf(1, 2, 3)
 * val letters = listOf('a', 'b', 'c')
 * numbers.crossProduct(letters) { number, letter ->
 *   // Each combination of (1, a), (1, b), (1, c), (2, a), (2, b), etc.
 * }
 * ```
 */
inline fun <T, U, R> Collection<T>.crossProduct(
    other: Collection<U>,
    block: (T, U) -> R,
) = flatMap { first ->
    other.map { second -> first to second }.map { block(it.first, it.second) }
}
