/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads.facts

import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.collect

/**
 * Facts emitted for telemetry related to [DownloadsFeature]
 */
class DownloadsFacts {
    /**
     * Items that specify which portion of the [DownloadsFeature] was interacted with
     */
    object Items {
        const val NOTIFICATION = "notification"
    }
}

internal fun emitNotificationResumeFact() = emitNotificationFact(Action.RESUME)
internal fun emitNotificationPauseFact() = emitNotificationFact(Action.PAUSE)
internal fun emitNotificationCancelFact() = emitNotificationFact(Action.CANCEL)
internal fun emitNotificationTryAgainFact() = emitNotificationFact(Action.TRY_AGAIN)
internal fun emitNotificationOpenFact() = emitNotificationFact(Action.OPEN)

private fun emitNotificationFact(
    action: Action,
) {
    Fact(
        Component.FEATURE_DOWNLOADS,
        action,
        DownloadsFacts.Items.NOTIFICATION,
    ).collect()
}
