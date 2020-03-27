/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.middleware.sideeffects

import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.MediaState
import mozilla.components.feature.media.facts.emitStatePauseFact
import mozilla.components.feature.media.facts.emitStatePlayFact
import mozilla.components.feature.media.facts.emitStateStopFact
import mozilla.components.support.base.facts.Fact

/**
 * Takes care of emitting [Fact]s whenever the [MediaState.State] changes.
 */
internal class MediaFactsEmitter {
    private var lastState = MediaState.State.NONE

    fun process(state: BrowserState) {
        if (lastState == state.media.aggregate.state) {
            return
        }

        when (state.media.aggregate.state) {
            MediaState.State.PLAYING -> emitStatePlayFact()

            MediaState.State.PAUSED -> emitStatePauseFact()

            MediaState.State.NONE -> emitStateStopFact()
        }

        lastState = state.media.aggregate.state
    }
}
