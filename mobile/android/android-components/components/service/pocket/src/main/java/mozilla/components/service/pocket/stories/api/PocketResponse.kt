/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.stories.api

/**
 * A response from the Pocket API: the subclasses determine the type of the result and contain usable data.
 */
internal sealed class PocketResponse<T> {

    /**
     * A successful response from the Pocket API.
     *
     * @param data The data returned from the Pocket API.
     */
    data class Success<T> internal constructor(val data: T) : PocketResponse<T>()

    /**
     * A failure response from the Pocket API.
     */
    class Failure<T> internal constructor() : PocketResponse<T>()

    companion object {

        /**
         * Wraps the given [target] in a [PocketResponse]: if [target] is
         * - null, then Failure
         * - a Collection and empty, then Failure
         * - a String and empty, then Failure
         * - a Boolean and false, then Failure
         * - otherwise, Success
         */
        internal fun <T : Any> wrap(target: T?): PocketResponse<T> = when (target) {
            null -> Failure()
            is Collection<*> -> if (target.isEmpty()) Failure() else Success(target)
            is String -> if (target.isBlank()) Failure() else Success(target)
            is Boolean -> if (target == false) Failure() else Success(target)
            else -> Success(target)
        }
    }
}
