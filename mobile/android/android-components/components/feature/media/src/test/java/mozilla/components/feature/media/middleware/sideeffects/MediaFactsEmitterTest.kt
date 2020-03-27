/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.middleware.sideeffects

import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.MediaState
import mozilla.components.feature.media.facts.MediaFacts
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.processor.CollectionProcessor
import org.junit.Assert.assertEquals
import org.junit.Test

class MediaFactsEmitterTest {
    @Test
    fun `Emits facts for current state`() {
        CollectionProcessor.withFactCollection { facts ->
            val emitter = MediaFactsEmitter()

            assertEquals(0, facts.size)

            emitter.process(BrowserState(media = MediaState(
                aggregate = MediaState.Aggregate(
                    state = MediaState.State.PLAYING
                )
            )))

            assertEquals(1, facts.size)
            facts[0].apply {
                assertEquals(Component.FEATURE_MEDIA, component)
                assertEquals(Action.PLAY, action)
                assertEquals(MediaFacts.Items.STATE, item)
            }

            emitter.process(BrowserState(media = MediaState(
                aggregate = MediaState.Aggregate(
                    state = MediaState.State.NONE
                )
            )))

            assertEquals(2, facts.size)
            facts[1].apply {
                assertEquals(Component.FEATURE_MEDIA, component)
                assertEquals(Action.STOP, action)
                assertEquals(MediaFacts.Items.STATE, item)
            }

            emitter.process(BrowserState(media = MediaState(
                aggregate = MediaState.Aggregate(
                    state = MediaState.State.NONE
                )
            )))

            assertEquals(2, facts.size)

            emitter.process(BrowserState(media = MediaState(
                aggregate = MediaState.Aggregate(
                    state = MediaState.State.PLAYING
                )
            )))

            assertEquals(3, facts.size)
            facts[2].apply {
                assertEquals(Component.FEATURE_MEDIA, component)
                assertEquals(Action.PLAY, action)
                assertEquals(MediaFacts.Items.STATE, item)
            }

            emitter.process(BrowserState(media = MediaState(
                aggregate = MediaState.Aggregate(
                    state = MediaState.State.PLAYING
                )
            )))

            assertEquals(3, facts.size)

            emitter.process(BrowserState(media = MediaState(
                aggregate = MediaState.Aggregate(
                    state = MediaState.State.PAUSED
                )
            )))

            assertEquals(4, facts.size)
            facts[3].apply {
                assertEquals(Component.FEATURE_MEDIA, component)
                assertEquals(Action.PAUSE, action)
                assertEquals(MediaFacts.Items.STATE, item)
            }

            emitter.process(BrowserState(media = MediaState(
                aggregate = MediaState.Aggregate(
                    state = MediaState.State.NONE
                )
            )))

            assertEquals(5, facts.size)
            facts[4].apply {
                assertEquals(Component.FEATURE_MEDIA, component)
                assertEquals(Action.STOP, action)
                assertEquals(MediaFacts.Items.STATE, item)
            }
        }
    }
}
