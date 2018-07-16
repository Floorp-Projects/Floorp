/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils.observer

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class ConsumableTest {
    @Test
    fun `Consumable created with from() is not consumed and contains value`() {
        val consumable = Consumable.from(42)
        assertFalse(consumable.isConsumed())
        assertEquals(42, consumable.internalValue)
    }

    @Test
    fun `Consumable created with empty() is already consumed and contains no value`() {
        val consumable = Consumable.empty<Int>()
        assertTrue(consumable.isConsumed())
        assertNull(consumable.internalValue)
    }

    @Test
    fun `value will not be consumed if consuming lambda returns false`() {
        val consumable = Consumable.from(42)

        assertEquals(42, consumable.internalValue)

        var lambdaExecuted = false

        val consumed = consumable.consume { value ->
            assertEquals(42, value)
            lambdaExecuted = true
            false // Do not consume value
        }

        assertFalse(consumed)
        assertTrue(lambdaExecuted)
        assertFalse(consumable.isConsumed())
        assertEquals(42, consumable.internalValue)
    }

    @Test
    fun `value will be consumed if consuming lambda returns true`() {
        val consumable = Consumable.from(42)

        assertEquals(42, consumable.internalValue)

        var lambdaExecuted = false

        val consumed = consumable.consume { value ->
            assertEquals(42, value)
            lambdaExecuted = true
            true // Consume value
        }

        assertTrue(consumed)
        assertTrue(lambdaExecuted)
        assertTrue(consumable.isConsumed())
        assertNull(consumable.internalValue)
    }

    @Test
    fun `value will not be consumed if all consuming lambdas do return false`() {
        val consumer1 = TestConsumer(shouldConsume = false)
        val consumer2 = TestConsumer(shouldConsume = false)
        val consumer3 = TestConsumer(shouldConsume = false)

        val consumable = Consumable.from(23)
        val consumed = consumable.consumeBy(listOf(
            { value -> consumer1.invoke(value) },
            { value -> consumer2.invoke(value) },
            { value -> consumer3.invoke(value) }
        ))

        assertFalse(consumed)
        assertFalse(consumable.isConsumed())
        assertEquals(23, consumable.internalValue)

        assertTrue(consumer1.callbackTriggered)
        assertEquals(23, consumer1.callbackValue)

        assertTrue(consumer2.callbackTriggered)
        assertEquals(23, consumer2.callbackValue)

        assertTrue(consumer3.callbackTriggered)
        assertEquals(23, consumer3.callbackValue)
    }

    @Test
    fun `value will be consumed if at least one lambdas returns true`() {
        val consumer1 = TestConsumer(shouldConsume = false)
        val consumer2 = TestConsumer(shouldConsume = true)
        val consumer3 = TestConsumer(shouldConsume = false)

        val consumable = Consumable.from(23)
        val consumed = consumable.consumeBy(listOf(
                { value -> consumer1.invoke(value) },
                { value -> consumer2.invoke(value) },
                { value -> consumer3.invoke(value) }
        ))

        assertTrue(consumed)
        assertTrue(consumable.isConsumed())
        assertNull(consumable.internalValue)

        assertTrue(consumer1.callbackTriggered)
        assertEquals(23, consumer1.callbackValue)

        assertTrue(consumer2.callbackTriggered)
        assertEquals(23, consumer2.callbackValue)

        assertTrue(consumer3.callbackTriggered)
        assertEquals(23, consumer3.callbackValue)
    }

    fun `value will be consumed if at multiple lambdas return true`() {
        val consumer1 = TestConsumer(shouldConsume = true)
        val consumer2 = TestConsumer(shouldConsume = false)
        val consumer3 = TestConsumer(shouldConsume = true)

        val consumable = Consumable.from(23)
        val consumed = consumable.consumeBy(listOf(
                { value -> consumer1.invoke(value) },
                { value -> consumer2.invoke(value) },
                { value -> consumer3.invoke(value) }
        ))

        assertTrue(consumed)
        assertTrue(consumable.isConsumed())
        assertNull(consumable.internalValue)

        assertTrue(consumer1.callbackTriggered)
        assertEquals(23, consumer1.callbackValue)

        assertTrue(consumer2.callbackTriggered)
        assertEquals(23, consumer2.callbackValue)

        assertTrue(consumer3.callbackTriggered)
        assertEquals(23, consumer3.callbackValue)
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
        assertNull(consumable.internalValue)

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
        assertNull(consumable.internalValue)

        val consumed = consumable.consumeBy(listOf(
                { value -> consumer1.invoke(value) },
                { value -> consumer2.invoke(value) },
                { value -> consumer3.invoke(value) }
        ))

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

    private class TestConsumer(
        private val shouldConsume: Boolean
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
