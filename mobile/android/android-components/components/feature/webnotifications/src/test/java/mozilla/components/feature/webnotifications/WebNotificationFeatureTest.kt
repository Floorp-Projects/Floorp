/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.webnotifications

import android.app.NotificationManager
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.icons.Icon
import mozilla.components.browser.icons.Icon.Source
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.permission.SitePermissions
import mozilla.components.concept.engine.permission.SitePermissions.Status
import mozilla.components.concept.engine.webnotifications.WebNotification
import mozilla.components.feature.sitepermissions.OnDiskSitePermissionsStorage
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.anyBoolean
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class WebNotificationFeatureTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private val context = spy(testContext)
    private val browserIcons: BrowserIcons = mock()
    private val icon: Icon = mock()
    private val engine: Engine = mock()
    private val notificationManager: NotificationManager = mock()
    private val permissionsStorage: OnDiskSitePermissionsStorage = mock()

    private val testNotification = WebNotification(
        "Mozilla",
        "mozilla.org",
        "Notification body",
        "mozilla.org",
        "https://mozilla.org/image.ico",
        "rtl",
        "en",
        false,
        mock(),
    )

    @Before
    fun setup() {
        `when`(context.getSystemService(NotificationManager::class.java)).thenReturn(notificationManager)
        `when`(icon.source).thenReturn(Source.GENERATOR) // to no-op the browser icons call.
        `when`(browserIcons.loadIcon(any())).thenReturn(CompletableDeferred(icon))
    }

    @Test
    fun `register web notification delegate`() {
        doNothing().`when`(engine).registerWebNotificationDelegate(any())
        doNothing().`when`(notificationManager).createNotificationChannel(any())

        WebNotificationFeature(context, engine, browserIcons, android.R.drawable.ic_dialog_alert, mock(), null)

        verify(engine).registerWebNotificationDelegate(any())
    }

    @Test
    fun `engine notifies to cancel notification`() {
        val webNotification: WebNotification = mock()
        val feature =
            WebNotificationFeature(context, engine, browserIcons, android.R.drawable.ic_dialog_alert, mock(), null)

        `when`(webNotification.tag).thenReturn("testTag")

        feature.onCloseNotification(webNotification)

        verify(notificationManager).cancel("testTag", NOTIFICATION_ID)
    }

    @Test
    fun `engine notifies to show notification`() = runTestOnMain {
        val notification = testNotification.copy(sourceUrl = "https://mozilla.org:443")
        val feature = WebNotificationFeature(
            context,
            engine,
            browserIcons,
            android.R.drawable.ic_dialog_alert,
            permissionsStorage,
            null,
            coroutineContext,
        )
        val permission = SitePermissions(origin = "https://mozilla.org:443", notification = Status.ALLOWED, savedAt = 0)

        `when`(permissionsStorage.findSitePermissionsBy(any(), anyBoolean())).thenReturn(permission)

        feature.onShowNotification(notification)

        verify(notificationManager).notify(eq(notification.tag), eq(NOTIFICATION_ID), any())
    }

    @Test
    fun `notification ignored if permissions are not allowed`() = runTestOnMain {
        val notification = testNotification.copy(sourceUrl = "https://mozilla.org:443")
        val feature = WebNotificationFeature(
            context,
            engine,
            browserIcons,
            android.R.drawable.ic_dialog_alert,
            permissionsStorage,
            null,
            coroutineContext,
        )

        // No permissions found.

        feature.onShowNotification(notification)

        verify(notificationManager, never()).notify(eq(testNotification.tag), eq(NOTIFICATION_ID), any())

        // When explicitly denied.

        val permission = SitePermissions(origin = "https://mozilla.org:443", notification = Status.BLOCKED, savedAt = 0)
        `when`(permissionsStorage.findSitePermissionsBy(any(), anyBoolean())).thenReturn(permission)

        feature.onShowNotification(testNotification)

        verify(notificationManager, never()).notify(eq(testNotification.tag), eq(NOTIFICATION_ID), any())
    }

    @Test
    fun `notifications always allowed for web extensions`() = runTestOnMain {
        val webExtensionNotification = WebNotification(
            "Mozilla",
            "mozilla.org",
            "Notification body",
            "mozilla.org",
            "https://mozilla.org/image.ico",
            "rtl",
            "en",
            false,
            mock(),
            triggeredByWebExtension = true,
        )

        val feature = WebNotificationFeature(
            context,
            engine,
            browserIcons,
            android.R.drawable.ic_dialog_alert,
            permissionsStorage,
            null,
            coroutineContext,
        )

        feature.onShowNotification(webExtensionNotification)
        verify(notificationManager).notify(eq(testNotification.tag), eq(NOTIFICATION_ID), any())
    }
}
