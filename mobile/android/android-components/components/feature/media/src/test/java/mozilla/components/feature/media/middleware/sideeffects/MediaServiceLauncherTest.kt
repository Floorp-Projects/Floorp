/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.middleware.sideeffects

import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.MediaState
import mozilla.components.feature.media.service.AbstractMediaService
import mozilla.components.support.test.mock
import org.junit.Test
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

class MediaServiceLauncherTest {
    @Test
    fun `PLAYING state launches service`() {
        val state = BrowserState(media = MediaState(
            aggregate = MediaState.Aggregate(
                state = MediaState.State.PLAYING
            )
        ))

        val launcher = spy(MediaServiceLauncher(mock(), AbstractMediaService::class.java))
        doNothing().`when`(launcher).launch()

        launcher.process(state)

        verify(launcher).launch()
    }

    @Test
    fun `PAUSED state does not launch service`() {
        val state = BrowserState(media = MediaState(
            aggregate = MediaState.Aggregate(
                state = MediaState.State.PAUSED
            )
        ))

        val launcher = spy(MediaServiceLauncher(mock(), AbstractMediaService::class.java))
        doNothing().`when`(launcher).launch()

        launcher.process(state)

        verify(launcher, never()).launch()
    }

    @Test
    fun `NONE state does not launch service`() {
        val state = BrowserState(media = MediaState(
            aggregate = MediaState.Aggregate(
                state = MediaState.State.NONE
            )
        ))

        val launcher = spy(MediaServiceLauncher(mock(), AbstractMediaService::class.java))
        doNothing().`when`(launcher).launch()

        launcher.process(state)

        verify(launcher, never()).launch()
    }
}
