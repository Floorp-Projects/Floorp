/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.facts

import mozilla.components.support.base.Component
import mozilla.components.support.test.mock
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito

class FactProcessorTest {
    @Before
    @After
    fun cleanUp() {
        Facts.clearProcessors()
    }

    @Test
    fun `register extension method regsiters processor on Facts singleton`() {
        val processor: FactProcessor = mock()
        processor.register()

        val fact = Fact(
            Component.SUPPORT_TEST,
            Action.CLICK,
            "test",
        )

        fact.collect()

        Mockito.verify(processor).process(fact)
    }
}
