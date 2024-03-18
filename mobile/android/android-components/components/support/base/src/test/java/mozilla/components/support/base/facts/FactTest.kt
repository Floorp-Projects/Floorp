/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.facts

import mozilla.components.support.base.Component
import mozilla.components.support.test.mock
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.verify

class FactTest {
    @Before
    @After
    fun cleanUp() {
        Facts.clearProcessors()
    }

    @Test
    fun `Collect extension method forwards Fact to Facts singleton`() {
        val processor: FactProcessor = mock()
        Facts.registerProcessor(processor)

        val fact = Fact(
            Component.SUPPORT_TEST,
            Action.CLICK,
            "test",
        )

        fact.collect()

        verify(processor).process(fact)
    }

    @Test
    fun `Accessing values`() {
        val fact = Fact(
            Component.SUPPORT_TEST,
            Action.CLICK,
            "test-item",
            "test-value",
            mapOf(
                "key1" to "value1",
                "key2" to "value2",
            ),
        )

        assertEquals(Component.SUPPORT_TEST, fact.component)
        assertEquals(Action.CLICK, fact.action)
        assertEquals("test-item", fact.item)
        assertEquals("test-value", fact.value)

        assertNotNull(fact.metadata)
        assertEquals(2, fact.metadata!!.size)
        assertTrue(fact.metadata!!.contains("key1"))
        assertTrue(fact.metadata!!.contains("key2"))
        assertEquals("value1", fact.metadata!!["key1"])
        assertEquals("value2", fact.metadata!!["key2"])
    }
}
