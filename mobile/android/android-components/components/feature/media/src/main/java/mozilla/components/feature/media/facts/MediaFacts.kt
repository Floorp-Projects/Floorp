/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.facts

import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.collect

/**
 * Facts emitted for telemetry related to [MediaFeature]
 */
class MediaFacts {
    /**
     * Items that specify which portion of the [MediaFeature] was interacted with
     */
    object Items {
        const val NOTIFICATION = "notification"
        const val STATE = "state"
    }
}

internal fun emitNotificationPlayFact() = emitNotificationFact(Action.PLAY)
internal fun emitNotificationPauseFact() = emitNotificationFact(Action.PAUSE)

internal fun emitStatePlayFact() = emitStateFact(Action.PLAY)
internal fun emitStatePauseFact() = emitStateFact(Action.PAUSE)
internal fun emitStateStopFact() = emitStateFact(Action.STOP)

private fun emitStateFact(
    action: Action,
) {
    Fact(
        Component.FEATURE_MEDIA,
        action,
        MediaFacts.Items.STATE,
    ).collect()
}

private fun emitNotificationFact(
    action: Action,
) {
    Fact(
        Component.FEATURE_MEDIA,
        action,
        MediaFacts.Items.NOTIFICATION,
    ).collect()
}
