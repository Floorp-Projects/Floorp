/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.media

import mozilla.components.browser.engine.gecko.GeckoEngineSession
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.media.Media
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.MediaElement

/**
 * [GeckoSession.MediaDelegate] implementation for wrapping [MediaElement] instances in [GeckoMedia] ([Media]) and
 * forwarding them to the [EngineSession.Observer].
 */
internal class GeckoMediaDelegate(
    private val engineSession: GeckoEngineSession
) : GeckoSession.MediaDelegate {
    private val mediaMap: MutableMap<MediaElement, Media> = mutableMapOf()

    override fun onMediaAdd(session: GeckoSession, element: MediaElement) {
        engineSession.notifyObservers {
            val media = GeckoMedia(element)

            mediaMap[element] = media

            onMediaAdded(media)
        }
    }

    override fun onMediaRemove(session: GeckoSession, element: MediaElement) {
        mediaMap[element]?.let { media ->
            engineSession.notifyObservers { onMediaRemoved(media) }
        }
    }
}
