/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.mediasession

import mozilla.components.support.test.mock
import org.junit.Test
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mozilla.geckoview.MediaSession as GeckoViewMediaSession

class GeckoMediaSessionControllerTest {
    @Test
    fun `GeckoMediaSessionController works correctly with GeckoView MediaSession`() {
        val geckoViewMediaSession: GeckoViewMediaSession = mock()
        val controller = GeckoMediaSessionController(geckoViewMediaSession)

        controller.pause()
        verify(geckoViewMediaSession, times(1)).pause()

        controller.stop()
        verify(geckoViewMediaSession, times(1)).stop()

        controller.play()
        verify(geckoViewMediaSession, times(1)).play()

        controller.seekTo(123.0, true)
        verify(geckoViewMediaSession, times(1)).seekTo(123.0, true)

        controller.seekForward()
        verify(geckoViewMediaSession, times(1)).seekForward()

        controller.seekBackward()
        verify(geckoViewMediaSession, times(1)).seekBackward()

        controller.nextTrack()
        verify(geckoViewMediaSession, times(1)).nextTrack()

        controller.previousTrack()
        verify(geckoViewMediaSession, times(1)).previousTrack()

        controller.skipAd()
        verify(geckoViewMediaSession, times(1)).skipAd()

        controller.muteAudio(true)
        verify(geckoViewMediaSession, times(1)).muteAudio(true)
    }
}
