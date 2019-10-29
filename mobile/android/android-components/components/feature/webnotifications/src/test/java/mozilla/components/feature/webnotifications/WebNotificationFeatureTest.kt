/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.webnotifications

import android.app.NotificationManager
import android.content.Context
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.concept.engine.Engine
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class WebNotificationFeatureTest {
    private lateinit var icons: BrowserIcons

    private val engine: Engine = mock()
    private val notificationManager: NotificationManager = mock()

    @Before
    fun setup() {
        icons = mock()
    }

    @Test
    fun `register web notification delegate`() {
        val context: Context = mock()
        doNothing().`when`(engine).registerWebNotificationDelegate(any())
        `when`(context.getSystemService(Context.NOTIFICATION_SERVICE)).thenReturn(notificationManager)
        doNothing().`when`(notificationManager).createNotificationChannel(any())
        WebNotificationFeature(context, engine, icons, android.R.drawable.ic_dialog_alert, null)

        verify(engine).registerWebNotificationDelegate(any())
    }
}
