/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.facts

/**
 * A [FactProcessor] receives [Fact] instances to process them further.
 */
interface FactProcessor {
    /**
     * Passes the given [Fact] to the [FactProcessor] for processing.
     */
    fun process(fact: Fact)
}

/**
 * Registers this [FactProcessor] to collect [Fact] instances from the [Facts] singleton.
 */
fun FactProcessor.register() = Facts.registerProcessor(this)
