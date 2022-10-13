/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.awesomebar

import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.collect

/**
 * Facts emitted for telemetry related to the AwesomeBar composable.
 */
object AwesomeBarFacts {
    /**
     * Specific types of telemetry items.
     */
    object Items {
        const val PROVIDER_DURATION = "provider_duration"
    }

    /**
     * Keys used to record metadata about [Items].
     */
    object MetadataKeys {
        const val DURATION_PAIR = "duration_pair"
    }

    internal fun emitAwesomeBarFact(
        action: Action,
        item: String,
        value: String? = null,
        metadata: Map<String, Any>? = null,
    ) {
        Fact(
            Component.COMPOSE_AWESOMEBAR,
            action,
            item,
            value,
            metadata,
        ).collect()
    }
}
