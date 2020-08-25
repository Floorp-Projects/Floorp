/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.feature

import android.content.Intent
import android.graphics.Color
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.icons.IconRequest
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.manifest.Size
import mozilla.components.concept.engine.manifest.WebAppManifest
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

        val feature = WebAppSiteControlsFeature(context, mock(), "session-id", mock())
        feature.onResume(mock())

        verify(context).registerReceiver(eq(feature), any())
    }

    @Test
    fun `unregister receiver on pause`() {
        val context = spy(testContext)

        doNothing().`when`(context).unregisterReceiver(any())

        val feature = WebAppSiteControlsFeature(context, mock(), "session-id", mock())
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

    @Test
    fun `load monochrome icon if defined in manifest`() {
        val sessionManager: SessionManager = mock()
        val session: Session = mock()
        val icons: BrowserIcons = mock()
        val manifest = WebAppManifest(
            name = "Mozilla",
            startUrl = "https://mozilla.org",
            scope = "https://mozilla.org",
            icons = listOf(
                WebAppManifest.Icon(
                    src = "https://mozilla.org/logo_color.svg",
                    sizes = listOf(Size.ANY),
                    type = "image/svg+xml",
                    purpose = setOf(WebAppManifest.Icon.Purpose.ANY, WebAppManifest.Icon.Purpose.MASKABLE)
                ),
                WebAppManifest.Icon(
                    src = "https://mozilla.org/logo_black.svg",
                    sizes = listOf(Size.ANY),
                    type = "image/svg+xml",
                    purpose = setOf(WebAppManifest.Icon.Purpose.MONOCHROME)
                )
            )
        )

        doReturn(session).`when`(sessionManager).findSessionById("session-id")

        val feature = WebAppSiteControlsFeature(testContext, sessionManager, "session-id", manifest, icons = icons)
        feature.onCreate()

        verify(icons).loadIcon(IconRequest(
            url = "https://mozilla.org",
            size = IconRequest.Size.DEFAULT,
            resources = listOf(IconRequest.Resource(
                url = "https://mozilla.org/logo_black.svg",
                type = IconRequest.Resource.Type.MANIFEST_ICON,
                sizes = listOf(Size.ANY),
                mimeType = "image/svg+xml",
                maskable = false
            )),
            color = Color.WHITE
        ))
    }
}
