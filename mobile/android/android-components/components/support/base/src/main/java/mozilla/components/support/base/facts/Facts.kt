/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.facts

import androidx.annotation.VisibleForTesting

/**
 * Global API for collecting [Fact] objects and forwarding them to [FactProcessor] instances.
 */
object Facts {
    private val processors = mutableListOf<FactProcessor>()

    /**
     * Registers a new [FactProcessor].
     */
    fun registerProcessor(processor: FactProcessor): Facts {
        processors.add(processor)
        return this
    }

    /**
     * Collects a [Fact] and forwards it to all registered [FactProcessor] instances.
     */
    fun collect(fact: Fact) {
        processors.forEach { it.process(fact) }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    fun clearProcessors() {
        processors.clear()
    }
}
