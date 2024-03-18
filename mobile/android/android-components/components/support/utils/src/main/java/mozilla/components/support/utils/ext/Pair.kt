/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils.ext

/**
 * @returns null if either [first] or [second] is null. Otherwise returns a [Pair] of non-null
 * values.
 *
 * Example:
 * (Object, Object).toNullablePair() == Pair<Object, Object>
 * (null, Object).toNullablePair() == null
 * (Object, null).toNullablePair() == null
 */
fun <T, U> Pair<T?, U?>.toNullablePair(): Pair<T, U>? =
    if (first != null && second != null) {
        first!! to second!!
    } else {
        null
    }
