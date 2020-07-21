/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa

import mozilla.components.feature.pwa.ProgressiveWebAppFacts.Companion.MS_PRECISION
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
        const val INSTALL_SHORTCUT = "install_shortcut"
        const val HOMESCREEN_ICON_TAP = "homescreen_icon_tap"
        const val ENTER_BACKGROUND = "enter_background"
        const val ENTER_FOREGROUND = "enter_foreground"
    }

    /**
     * Keys used to record metadata about [Items].
     */
    object MetadataKeys {
        const val BACKGROUND_TIME = "background_time"
        const val FOREGROUND_TIME = "foreground_time"
    }

    companion object {
        // We only care about millisecond precision in this context
        internal const val MS_PRECISION = 1_000_000L
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

internal fun emitPwaInstallFact() =
    emitPwaFact(
        Action.CLICK,
        ProgressiveWebAppFacts.Items.INSTALL_SHORTCUT
    )

internal fun emitForegroundTimingFact(timingNs: Long) =
    emitPwaFact(
        Action.INTERACTION,
        ProgressiveWebAppFacts.Items.ENTER_FOREGROUND,
        metadata = mapOf(
            ProgressiveWebAppFacts.MetadataKeys.FOREGROUND_TIME to (timingNs / MS_PRECISION)
        )
    )

internal fun emitBackgroundTimingFact(timingNs: Long) =
    emitPwaFact(
        Action.INTERACTION,
        ProgressiveWebAppFacts.Items.ENTER_BACKGROUND,
        metadata = mapOf(
            ProgressiveWebAppFacts.MetadataKeys.BACKGROUND_TIME to (timingNs / MS_PRECISION)
        )
    )
