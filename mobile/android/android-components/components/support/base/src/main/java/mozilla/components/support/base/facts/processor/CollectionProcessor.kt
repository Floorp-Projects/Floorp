/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.facts.processor

import androidx.annotation.VisibleForTesting
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.FactProcessor
import mozilla.components.support.base.facts.Facts

/**
 * A [FactProcessor] implementation that keeps all [Fact] objects in a list.
 *
 * This [FactProcessor] is only for testing.
 */
@VisibleForTesting
class CollectionProcessor : FactProcessor {
    private val internalFacts = mutableListOf<Fact>()

    val facts: List<Fact>
        get() = internalFacts

    override fun process(fact: Fact) {
        internalFacts.add(fact)
    }

    companion object {
        /**
         * Helper for creating a [CollectionProcessor], registering it and clearing the processors again.
         *
         * Use in tests like:
         *
         * ```
         * CollectionProcessor.withFactCollection { facts ->
         *   // During execution of this block the "facts" list will be updated automatically to contain
         *   // all facts that were emitted while executing this block.
         *   // After this block has completed all registered processors will be cleared.
         * }
         * ```
         */
        fun withFactCollection(block: (List<Fact>) -> Unit) {
            val processor = CollectionProcessor()

            try {
                Facts.registerProcessor(processor)

                block.invoke(processor.facts)
            } finally {
                Facts.clearProcessors()
            }
        }
    }
}
