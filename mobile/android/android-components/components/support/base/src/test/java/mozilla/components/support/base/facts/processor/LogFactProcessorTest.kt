/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.facts.processor

import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.test.mock
import org.junit.Test
import org.mockito.Mockito.verify

class LogFactProcessorTest {
    @Test
    fun `Processor forwards Fact objects to the log`() {
        val logger: Logger = mock()
        val processor = LogFactProcessor(logger)

        val fact = Fact(
            Component.SUPPORT_TEST,
            Action.CLICK,
            "test",
        )

        processor.process(fact)

        verify(logger).debug(fact.toString())
    }
}
