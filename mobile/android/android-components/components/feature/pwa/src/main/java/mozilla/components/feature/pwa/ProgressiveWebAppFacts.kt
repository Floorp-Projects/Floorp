/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa

import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.collect

/**
 * Facts emitted for telemetry related to [PwaFeature]
 */
class ProgressiveWebAppFacts {
    /**
     * Items that specify which portion of the [PwaFeature] was interacted with
     */
    object Items {
        const val HOMESCREEN_ICON_TAP = "homescreen_icon_tap"
        const val BACKGROUND_TIMING = "background_timing"
    }

    /**
     * Keys used to record metadata about [Items].
     */
    object MetadataKeys {
        const val BACKGROUND_DURATION = "background_duration"
    }
}

private fun emitPwaFact(
    action: Action,
    item: String,
    value: String? = null,
    metadata: Map<String, Any>? = null
) {
    Fact(
        Component.FEATURE_PWA,
        action,
        item,
        value,
        metadata
    ).collect()
}

internal fun emitHomescreenIconTapFact() =
    emitPwaFact(
        Action.CLICK,
        ProgressiveWebAppFacts.Items.HOMESCREEN_ICON_TAP
    )

@Suppress("MagicNumber")
internal fun emitBackgroundTimingFact(timingNs: Long) =
    emitPwaFact(
        Action.INTERACTION,
        ProgressiveWebAppFacts.Items.BACKGROUND_TIMING,
        metadata = mapOf(
            // We only care about millisecond precision here, so convert from ns to ms before emitting.
            ProgressiveWebAppFacts.MetadataKeys.BACKGROUND_DURATION to (timingNs / 1_000_000L)
        )
    )
