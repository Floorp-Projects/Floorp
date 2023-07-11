/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads.facts

import mozilla.components.feature.downloads.facts.DownloadsFacts.Items.PROMPT
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
        const val PROMPT = "prompt"
    }
}

internal fun emitNotificationResumeFact() = emitFact(Action.RESUME)
internal fun emitNotificationPauseFact() = emitFact(Action.PAUSE)
internal fun emitNotificationCancelFact() = emitFact(Action.CANCEL)
internal fun emitNotificationTryAgainFact() = emitFact(Action.TRY_AGAIN)
internal fun emitNotificationOpenFact() = emitFact(Action.OPEN)
internal fun emitPromptDisplayedFact() = emitFact(Action.DISPLAY, item = PROMPT)
internal fun emitPromptDismissedFact() = emitFact(Action.CANCEL, item = PROMPT)

private fun emitFact(
    action: Action,
    item: String = DownloadsFacts.Items.NOTIFICATION,
) {
    Fact(
        Component.FEATURE_DOWNLOADS,
        action,
        item,
    ).collect()
}
