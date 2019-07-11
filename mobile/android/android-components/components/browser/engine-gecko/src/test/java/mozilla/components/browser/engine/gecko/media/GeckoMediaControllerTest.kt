/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.media

import mozilla.components.support.test.mock
import org.junit.Test
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions
import org.mozilla.geckoview.MediaElement

class GeckoMediaControllerTest {
    @Test
    fun `Pause is forwarded to media element`() {
        val mediaElement: MediaElement = mock()

        val controller = GeckoMediaController(mediaElement)
        controller.pause()

        verify(mediaElement).pause()
    }

    @Test
    fun `Play is forwarded to media element`() {
        val mediaElement: MediaElement = mock()

        val controller = GeckoMediaController(mediaElement)
        controller.play()

        verify(mediaElement).play()
    }

    @Test
    fun `Seek is forwarded to media element`() {
        val mediaElement: MediaElement = mock()

        val controller = GeckoMediaController(mediaElement)
        controller.seek(1337.42)

        verify(mediaElement).seek(1337.42)
    }

    @Test
    fun `SetMuted is forwarded to media element`() {
        val mediaElement: MediaElement = mock()

        val controller = GeckoMediaController(mediaElement)
        controller.setMuted(true)

        verify(mediaElement).setMuted(true)
        verifyNoMoreInteractions(mediaElement)

        controller.setMuted(false)
        verify(mediaElement).setMuted(false)
    }
}
