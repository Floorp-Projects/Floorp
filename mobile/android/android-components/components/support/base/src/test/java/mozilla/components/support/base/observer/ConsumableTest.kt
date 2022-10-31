/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.observer

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class ConsumableTest {
    @Test
    fun `Consumable created with from() is not consumed and contains value`() {
        val consumable = Consumable.from(42)
        assertFalse(consumable.isConsumed())
        assertEquals(42, consumable.value)
    }

    @Test
    fun `Consumable created with empty() is already consumed and contains no value`() {
        val consumable = Consumable.empty<Int>()
        assertTrue(consumable.isConsumed())
        assertNull(consumable.value)
    }

    @Test
    fun `value will not be consumed if consuming lambda returns false`() {
        val consumable = Consumable.from(42)

        assertEquals(42, consumable.value)

        var lambdaExecuted = false

        val consumed = consumable.consume { value ->
            assertEquals(42, value)
            lambdaExecuted = true
            false // Do not consume value
        }

        assertFalse(consumed)
        assertTrue(lambdaExecuted)
        assertFalse(consumable.isConsumed())
        assertEquals(42, consumable.value)
    }

    @Test
    fun `value will be consumed if consuming lambda returns true`() {
        val consumable = Consumable.from(42)

        assertEquals(42, consumable.value)

        var lambdaExecuted = false

        val consumed = consumable.consume { value ->
            assertEquals(42, value)
            lambdaExecuted = true
            true // Consume value
        }

        assertTrue(consumed)
        assertTrue(lambdaExecuted)
        assertTrue(consumable.isConsumed())
        assertNull(consumable.value)
    }

    @Test
    fun `value will not be consumed if all consuming lambdas do return false`() {
        val consumer1 = TestConsumer(shouldConsume = false)
        val consumer2 = TestConsumer(shouldConsume = false)
        val consumer3 = TestConsumer(shouldConsume = false)

        val consumable = Consumable.from(23)
        val consumed = consumable.consumeBy(
            listOf(
                { value -> consumer1.invoke(value) },
                { value -> consumer2.invoke(value) },
                { value -> consumer3.invoke(value) },
            ),
        )

        assertFalse(consumed)
        assertFalse(consumable.isConsumed())
        assertEquals(23, consumable.value)

        assertTrue(consumer1.callbackTriggered)
        assertEquals(23, consumer1.callbackValue)

        assertTrue(consumer2.callbackTriggered)
        assertEquals(23, consumer2.callbackValue)

        assertTrue(consumer3.callbackTriggered)
        assertEquals(23, consumer3.callbackValue)
    }

    @Test
    fun `value will be consumed if at least one lambda returns true`() {
        val consumer1 = TestConsumer(shouldConsume = false)
        val consumer2 = TestConsumer(shouldConsume = true)
        val consumer3 = TestConsumer(shouldConsume = false)

        val consumable = Consumable.from(23)
        val consumed = consumable.consumeBy(
            listOf(
                { value -> consumer1.invoke(value) },
                { value -> consumer2.invoke(value) },
                { value -> consumer3.invoke(value) },
            ),
        )

        assertTrue(consumed)
        assertTrue(consumable.isConsumed())
        assertNull(consumable.value)

        assertTrue(consumer1.callbackTriggered)
        assertEquals(23, consumer1.callbackValue)

        assertTrue(consumer2.callbackTriggered)
        assertEquals(23, consumer2.callbackValue)

        assertTrue(consumer3.callbackTriggered)
        assertEquals(23, consumer3.callbackValue)
    }

    @Test
    fun `value will be consumed if multiple lambdas return true`() {
        val consumer1 = TestConsumer(shouldConsume = true)
        val consumer2 = TestConsumer(shouldConsume = false)
        val consumer3 = TestConsumer(shouldConsume = true)

        val consumable = Consumable.from(23)
        val consumed = consumable.consumeBy(
            listOf(
                { value -> consumer1.invoke(value) },
                { value -> consumer2.invoke(value) },
                { value -> consumer3.invoke(value) },
            ),
        )

        assertTrue(consumed)
        assertTrue(consumable.isConsumed())
        assertNull(consumable.value)

        assertTrue(consumer1.callbackTriggered)
        assertEquals(23, consumer1.callbackValue)

        assertTrue(consumer2.callbackTriggered)
        assertEquals(23, consumer2.callbackValue)

        assertTrue(consumer3.callbackTriggered)
        assertEquals(23, consumer3.callbackValue)
    }

    @Test
    fun `value can be read and not consumed`() {
        val consumable = Consumable.from(42)

        assertEquals(42, consumable.peek())
        assertFalse(consumable.isConsumed())

        consumable.consume { true }
        assertNull(consumable.peek())
    }

    @Test
    fun `listeners are notified when value is consumed`() {
        var listener1Notified = false
        var listener2Notified = false

        val consumable = Consumable.from(42) { listener1Notified = true }
        consumable.onConsume { listener2Notified = true }

        assertFalse(listener1Notified)
        assertFalse(listener2Notified)

        consumable.consume { true }
        assertTrue(listener1Notified)
        assertTrue(listener2Notified)
    }

    @Test
    fun `calling consume on an already consumed consumable does nothing`() {
        val consumable = Consumable.from(42)

        val firstConsumer = TestConsumer(shouldConsume = true)
        val consumed = consumable.consume(firstConsumer::invoke)

        assertTrue(consumed)
        assertTrue(firstConsumer.callbackTriggered)
        assertEquals(42, firstConsumer.callbackValue)

        assertTrue(consumable.isConsumed())
        assertNull(consumable.value)

        val secondConsumer = TestConsumer(shouldConsume = true)
        val consumed2 = consumable.consume(secondConsumer::invoke)

        assertFalse(consumed2)
        assertFalse(secondConsumer.callbackTriggered)
        assertNull(secondConsumer.callbackValue)
    }

    @Test
    fun `calling consumeBy on an already consumed consumable does nothing`() {
        val consumer1 = TestConsumer(shouldConsume = true)
        val consumer2 = TestConsumer(shouldConsume = false)
        val consumer3 = TestConsumer(shouldConsume = true)

        val consumable = Consumable.from(23)
        assertTrue(consumable.consume { true })

        assertTrue(consumable.isConsumed())
        assertNull(consumable.value)

        val consumed = consumable.consumeBy(
            listOf(
                { value -> consumer1.invoke(value) },
                { value -> consumer2.invoke(value) },
                { value -> consumer3.invoke(value) },
            ),
        )

        assertFalse(consumed)

        assertFalse(consumer1.callbackTriggered)
        assertNull(consumer1.callbackValue)

        assertFalse(consumer2.callbackTriggered)
        assertNull(consumer2.callbackValue)

        assertFalse(consumer3.callbackTriggered)
        assertNull(consumer3.callbackValue)
    }

    @Test
    fun `value will not be consumed if list of consumers is empty`() {
        val consumable = Consumable.from(23)

        assertFalse(consumable.isConsumed())

        consumable.consumeBy(emptyList())

        assertFalse(consumable.isConsumed())
    }

    @Test
    fun `callback gets invoked if value gets consumed`() {
        var callbackInvoked = false

        val consumable = Consumable.from(42) {
            callbackInvoked = true
        }

        consumable.consume { false }

        assertFalse(callbackInvoked)

        consumable.consume { true }

        assertTrue(callbackInvoked)
    }

    @Test
    fun `callback gets invoked if one consumer in list consumes value`() {
        var callbackInvoked = false

        val consumable = Consumable.from(42) {
            callbackInvoked = true
        }

        consumable.consumeBy(
            listOf<(Int) -> Boolean>(
                { false },
                { false },
                { false },
                { false },
            ),
        )

        assertFalse(callbackInvoked)

        consumable.consumeBy(
            listOf<(Int) -> Boolean>(
                { false },
                { false },
                { true },
                { false },
            ),
        )

        assertTrue(callbackInvoked)
    }

    @Test
    fun `stream gets consumed in insertion order`() {
        val stream = Consumable.stream(1, 2, 3)
        val consumed = mutableListOf<Int>()
        stream.consumeAll { consumed.add(it) }
        assertEquals(listOf(1, 2, 3), consumed)
    }

    @Test
    fun `stream can get consumed in multiple steps`() {
        val stream = Consumable.stream(1, 2, 3)
        val consumed = mutableListOf<Int>()
        stream.consumeAll { value -> if (value < 3) consumed.add(value) else false }
        assertEquals(listOf(1, 2), consumed)

        stream.consumeAll { consumed.add(it) }
        assertEquals(listOf(1, 2, 3), consumed)
    }

    @Test
    fun `stream elements get consumed in insertion order`() {
        val stream = Consumable.stream(1, 2, 3)
        val consumed = mutableListOf<Int>()
        stream.consumeNext { consumed.add(it) }
        assertEquals(listOf(1), consumed)

        stream.consumeNext { consumed.add(it) }
        assertEquals(listOf(1, 2), consumed)

        stream.consumeNext { consumed.add(it) }
        assertEquals(listOf(1, 2, 3), consumed)
    }

    @Test
    fun `stream can be consumed by multiple consumers`() {
        var stream = Consumable.stream(1, 2, 3)
        var consumed = mutableListOf<Int>()

        // Consume all values using two consumers
        stream.consumeAllBy(
            listOf(
                { value -> consumed.add(value) },
                { value -> consumed.add(value + 1) },
            ),
        )
        assertEquals(listOf(1, 2, 2, 3, 3, 4), consumed)
        assertTrue(stream.isConsumed())

        // Consume partial values
        stream = Consumable.stream(1, 2, 3)
        consumed = mutableListOf()
        var allConsumed = stream.consumeAllBy(
            listOf(
                { value -> if (value < 3) consumed.add(value) else false },
                { _ -> false },
            ),
        )
        assertEquals(listOf(1, 2), consumed)
        assertFalse(allConsumed)
        assertFalse(stream.isConsumed())

        // Consume remaining values
        allConsumed = stream.consumeAllBy(
            listOf(
                { value -> consumed.add(value) },
                { _ -> false },
            ),
        )
        assertEquals(listOf(1, 2, 3), consumed)
        assertTrue(allConsumed)
        assertTrue(stream.isConsumed())

        // Consume no values
        stream = Consumable.stream(1, 2, 3)
        stream.consumeAllBy(
            listOf(
                { _ -> false },
                { _ -> false },
            ),
        )
        assertFalse(stream.isConsumed())
    }

    @Test
    fun `stream elements can be consumed by multiple consumers`() {
        val stream = Consumable.stream(1, 2, 3)
        val consumed = mutableListOf<Int>()
        stream.consumeNextBy(
            listOf(
                { value -> consumed.add(value) },
                { value -> consumed.add(value + 1) },
            ),
        )
        assertEquals(listOf(1, 2), consumed)

        stream.consumeNextBy(
            listOf(
                { value -> consumed.add(value + 1) },
                { _ -> false },
            ),
        )
        assertEquals(listOf(1, 2, 3), consumed)

        stream.consumeNextBy(
            listOf(
                { _ -> false },
                { _ -> false },
            ),
        )
        assertFalse(stream.isConsumed())

        stream.consumeNextBy(
            listOf(
                { _ -> false },
                { value -> consumed.add(value + 1) },
            ),
        )
        assertEquals(listOf(1, 2, 3, 4), consumed)

        assertTrue(stream.isConsumed())
        assertFalse(
            stream.consumeNextBy(
                listOf(
                    { _ -> true },
                ),
            ),
        )
    }

    @Test
    fun `stream is consumed when all values are consumed`() {
        val stream = Consumable.stream(1, 2)
        assertFalse(stream.isConsumed())

        stream.consumeNext { true }
        stream.consumeNext { true }
        assertTrue(stream.isConsumed())

        assertFalse(stream.consumeNext { true })
    }

    @Test
    fun `stream retains consumed values`() {
        val stream = Consumable.stream(1, 2)
        assertFalse(stream.isEmpty())

        stream.consumeNext { true }
        stream.consumeNext { true }
        assertFalse(stream.isEmpty())
    }

    @Test
    fun `stream can be appended`() {
        var stream = Consumable.stream(1)
        val consumed = mutableListOf<Int>()
        stream.consumeNext { consumed.add(it) }
        assertEquals(listOf(1), consumed)
        assertTrue(stream.isConsumed())

        stream = stream.append(2)
        assertFalse(stream.isConsumed())

        stream.consumeNext { consumed.add(it) }
        assertEquals(listOf(1, 2), consumed)
        assertTrue(stream.isConsumed())
    }

    @Test
    fun `values can be removed from stream`() {
        var stream = Consumable.stream(1, 2)
        val consumed = mutableListOf<Int>()
        stream.consumeNext { consumed.add(it) }
        assertEquals(listOf(1), consumed)
        assertFalse(stream.isConsumed())

        stream = stream.remove(2)
        assertTrue(stream.isConsumed())

        assertFalse(stream.consumeNext { consumed.add(it) })
        assertEquals(listOf(1), consumed)
    }

    @Test
    fun `consumed values can be removed from stream`() {
        var stream = Consumable.stream(1, 2)
        val consumed = mutableListOf<Int>()
        stream.consumeNext { consumed.add(it) }
        assertEquals(listOf(1), consumed)

        stream = stream.removeConsumed()
        assertFalse(stream.isEmpty())

        stream.consumeNext { consumed.add(it) }
        assertEquals(listOf(1, 2), consumed)

        stream = stream.removeConsumed()
        assertTrue(stream.isEmpty())
    }

    private class TestConsumer(
        private val shouldConsume: Boolean,
    ) {
        var callbackTriggered = false
        var callbackValue: Int? = null

        fun invoke(value: Int): Boolean {
            callbackTriggered = true
            callbackValue = value
            return shouldConsume
        }
    }
}
