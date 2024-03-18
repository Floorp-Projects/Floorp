/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.facts.processor

import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.FactProcessor
import mozilla.components.support.base.log.logger.Logger

/**
 * A [FactProcessor] implementation that prints collected [Fact] instances to the log.
 */
class LogFactProcessor(
    private val logger: Logger = Logger("Facts"),
) : FactProcessor {

    override fun process(fact: Fact) {
        logger.debug(fact.toString())
    }
}
