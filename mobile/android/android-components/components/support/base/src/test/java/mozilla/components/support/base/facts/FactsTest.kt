/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.facts

import mozilla.components.support.base.Component
import mozilla.components.support.test.mock
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.verify

class FactsTest {
    @Before
    @After
    fun cleanUp() {
        Facts.clearProcessors()
    }

    @Test
    fun `Collecting fact without processor`() {
        Facts.collect(
            Fact(
                Component.SUPPORT_TEST,
                Action.CLICK,
                "test",
            ),
        )
    }

    @Test
    fun `Collected fact is forwarded to processors`() {
        val processor1: FactProcessor = mock()
        val processor2: FactProcessor = mock()

        Facts
            .registerProcessor(processor1)
            .registerProcessor(processor2)

        val fact1 = Fact(
            Component.SUPPORT_TEST,
            Action.CLICK,
            "test",
        )

        Facts.collect(fact1)

        verify(processor1).process(fact1)
        verify(processor2).process(fact1)

        val fact2 = Fact(
            Component.SUPPORT_BASE,
            Action.TOGGLE,
            "test",
        )

        Facts.collect(fact2)

        verify(processor1).process(fact2)
        verify(processor2).process(fact2)
    }
}
