/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils.observer

import android.support.annotation.VisibleForTesting

/**
 * A generic wrapper for values that can get consumed.
 */
class Consumable<T> private constructor(
    private var value: T?
) {
    // Internal getter for testing only.
    @VisibleForTesting internal val internalValue: T?
        get() = value

    /**
     * Invokes the given lambda and marks the value as consumed if the lambda returns true.
     */
    @Synchronized
    fun consume(consumer: (value: T) -> Boolean): Boolean {
        return if (value?.let(consumer) == true) {
            value = null
            true
        } else {
            false
        }
    }

    /**
     * Invokes the given list of lambdas and marks the value as consumed if at least one lambda
     * returned true.
     */
    @Synchronized
    fun consumeBy(consumers: List<(T) -> Boolean>): Boolean {
        val value = value ?: return false
        val results = consumers.map { consumer -> consumer(value) }

        return if (results.contains(true)) {
            this.value = null
            true
        } else {
            false
        }
    }

    /**
     * Returns whether the value was consumed.
     */
    @Synchronized
    fun isConsumed() = value == null

    companion object {
        /**
         * Create a new Consumable wrapping the given value.
         */
        fun <T> from(value: T): Consumable<T> = Consumable(value)

        /**
         * Returns an empty Consumable with not value as if it was consumed already.
         */
        fun <T> empty(): Consumable<T> = Consumable(null)
    }
}
