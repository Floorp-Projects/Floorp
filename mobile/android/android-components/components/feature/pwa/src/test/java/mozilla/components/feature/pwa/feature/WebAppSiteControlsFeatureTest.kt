/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.feature

import android.content.Intent
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class WebAppSiteControlsFeatureTest {

    @Test
    fun `register receiver on resume`() {
        val context = spy(testContext)

        val feature = WebAppSiteControlsFeature(context, mock(), mock(), "session-id", mock())
        feature.onResume()

        verify(context).registerReceiver(eq(feature), any())
    }

    @Test
    fun `unregister receiver on pause`() {
        val context = spy(testContext)

        doNothing().`when`(context).unregisterReceiver(any())

        val feature = WebAppSiteControlsFeature(context, mock(), mock(), "session-id", mock())
        feature.onPause()

        verify(context).unregisterReceiver(feature)
    }

    @Test
    fun `reload page when reload action is activated`() {
        val sessionManager: SessionManager = mock()
        val session: Session = mock()
        val reloadUrlUseCase: SessionUseCases.ReloadUrlUseCase = mock()

        doReturn(session).`when`(sessionManager).findSessionById("session-id")

        val feature = WebAppSiteControlsFeature(testContext, sessionManager, reloadUrlUseCase, "session-id", mock())
        feature.onReceive(testContext, Intent("mozilla.components.feature.pwa.REFRESH"))

        verify(reloadUrlUseCase).invoke(session)
    }
}
