/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.observer

typealias ConsumableListener = () -> Unit

/**
 * A generic wrapper for values that can get consumed.
 *
 * @param value The value to be wrapped.
 * @param onConsume A callback that gets invoked if the wrapped value gets consumed.
 */
class Consumable<T> private constructor(
    internal var value: T?,
    onConsume: ConsumableListener? = null,
) {

    private val listeners = mutableSetOf<ConsumableListener>().also { listeners ->
        if (onConsume != null) {
            listeners.add(onConsume)
        }
    }

    /**
     * Invokes the given lambda and marks the value as consumed if the lambda returns true.
     */
    @Synchronized
    fun consume(consumer: (value: T) -> Boolean): Boolean {
        return if (value?.let(consumer) == true) {
            value = null
            listeners.forEach { it() }
            true
        } else {
            false
        }
    }

    /**
     * Invokes the given list of lambdas and marks the value as consumed if at least one lambda
     * returns true.
     */
    @Synchronized
    fun consumeBy(consumers: List<(T) -> Boolean>): Boolean {
        val value = value ?: return false
        val results = consumers.map { consumer -> consumer(value) }

        return if (results.contains(true)) {
            this.value = null
            listeners.forEach { it() }
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

    /**
     * Returns the value of this [Consumable] without consuming it.
     */
    @Synchronized
    fun peek(): T? = value

    /**
     * Adds a listener to be invoked when this [Consumable] is consumed.
     *
     * @param listener the listener to add.
     */
    @Synchronized
    fun onConsume(listener: ConsumableListener) {
        listeners.add(listener)
    }

    companion object {
        /**
         * Creates a new Consumable wrapping the given value.
         */
        fun <T> from(value: T, onConsume: (() -> Unit)? = null): Consumable<T> = Consumable(value, onConsume)

        /**
         * Creates a new Consumable stream for the provided values.
         */
        fun <T> stream(vararg values: T): ConsumableStream<T> = ConsumableStream(
            values.map { Consumable(it) },
        )

        /**
         * Returns an empty Consumable with not value as if it was consumed already.
         */
        fun <T> empty(): Consumable<T> = Consumable(null)
    }
}

/**
 * A generic wrapper for a stream of values that can be consumed. Values will
 * be consumed first in, first out.
 */
class ConsumableStream<T> internal constructor(private val consumables: List<Consumable<T>>) {

    /**
     * Invokes the given lambda with the next consumable value and marks the value
     * as consumed if the lambda returns true.
     *
     * @param consumer a lambda accepting a consumable value.
     * @return true if the consumable was consumed, otherwise false.
     */
    @Synchronized
    fun consumeNext(consumer: (value: T) -> Boolean): Boolean {
        val consumable = consumables.find { !it.isConsumed() }
        return consumable?.consume(consumer) ?: false
    }

    /**
     * Invokes the given lambda for each consumable value and marks the values
     * as consumed if the lambda returns true.
     *
     * @param consumer a lambda accepting a consumable value.
     * @return true if all consumables were consumed, otherwise false.
     */
    @Synchronized
    fun consumeAll(consumer: (value: T) -> Boolean): Boolean {
        val results = consumables.map { if (!it.isConsumed()) it.consume(consumer) else true }
        return !results.contains(false)
    }

    /**
     * Invokes the given list of lambdas with the next consumable value and marks the
     * value as consumed if at least one lambda returns true.
     *
     * @param consumers the lambdas accepting the next consumable value.
     * @return true if the consumable was consumed, otherwise false.
     */
    @Synchronized
    fun consumeNextBy(consumers: List<(T) -> Boolean>): Boolean {
        val consumable = consumables.find { !it.isConsumed() }
        return consumable?.consumeBy(consumers) ?: false
    }

    /**
     * Invokes the given list of lambdas for each consumable value and marks the
     * values as consumed if at least one lambda returns true.
     *
     * @param consumers the lambdas accepting a consumable value.
     * @return true if all consumables were consumed, otherwise false.
     */
    @Synchronized
    fun consumeAllBy(consumers: List<(T) -> Boolean>): Boolean {
        val results = consumables.map { if (!it.isConsumed()) it.consumeBy(consumers) else true }
        return !results.contains(false)
    }

    /**
     * Copies the stream and appends the provided values.
     *
     * @param values the values to append.
     * @return a new consumable stream with the values appended.
     */
    @Synchronized
    fun append(vararg values: T): ConsumableStream<T> =
        ConsumableStream(consumables + values.map { Consumable.from(it) })

    /**
     * Copies the stream but removes all consumables equal to the provided value.
     *
     * @param value the value to remove.
     * @return a new consumable stream with the matching values removed.
     */
    @Synchronized
    fun remove(value: T): ConsumableStream<T> =
        ConsumableStream(consumables.filterNot { it.value == value })

    /**
     * Copies the stream but removes all consumed values.
     *
     * @return a new consumable stream with the consumed values removed.
     */
    @Synchronized
    fun removeConsumed(): ConsumableStream<T> =
        ConsumableStream(consumables.filterNot { it.isConsumed() })

    /**
     * Returns true if all values in this stream were consumed, otherwise false.
     */
    @Synchronized
    fun isConsumed() = consumables.filterNot { it.isConsumed() }.isEmpty()

    /**
     * Returns true if the stream is empty, otherwise false.
     */
    @Synchronized
    fun isEmpty() = consumables.isEmpty()
}
