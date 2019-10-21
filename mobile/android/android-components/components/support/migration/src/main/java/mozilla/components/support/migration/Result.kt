/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration

/**
 * Class representing the result of a successful or failed migration action.
 */
sealed class Result<T> {
    /**
     * Successful migration that produced data of type [T].
     *
     * @property value The result of the successful migration.
     */
    class Success<T>(
        val value: T
    ) : Result<T>()

    /**
     * Failed migration.
     *
     * @property throwables The [Throwable]s that caused this migration to fail.
     */
    class Failure<T>(
        val throwables: List<Throwable>
    ) : Result<T>() {
        constructor(throwable: Throwable) : this(listOf(throwable))
    }
}
