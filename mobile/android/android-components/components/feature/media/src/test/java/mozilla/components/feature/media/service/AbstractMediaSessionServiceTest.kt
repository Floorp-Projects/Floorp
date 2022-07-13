/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.service

import android.app.Service.START_NOT_STICKY
import android.content.ComponentName
import android.content.Intent
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify
import org.robolectric.Robolectric

@RunWith(AndroidJUnit4::class)
class AbstractMediaSessionServiceTest {
    private val service = Robolectric.buildService(FakeMediaService::class.java).get()

    @Test
    fun `WHEN the service is created THEN it initialize all needed properties`() {
        service.onCreate()

        assertNotNull(service.binder)
        assertEquals(MediaSessionServiceDelegate::class.java, service.delegate!!.javaClass)
        assertEquals(service, service.delegate!!.context)
        assertEquals(service, service.delegate!!.service)
        assertEquals(service.store, service.delegate!!.store)
    }

    @Test
    fun `WHEN the service is destroyed THEN clean all internal properties`() {
        service.delegate = mock()
        service.binder = mock()

        service.onDestroy()

        assertNull(service.delegate)
        assertNull(service.binder)
    }

    @Test
    fun `WHEN the service receives a new Intent THEN send it to the delegate and set the service to not be automatically restarted`() {
        service.delegate = mock()
        val intent: Intent = mock()

        val result = service.onStartCommand(intent, 33, 22)

        verify(service.delegate)!!.onStartCommand(intent)
        assertEquals(START_NOT_STICKY, result)
    }

    @Test
    fun `GIVEN the service is running WHEN a task in the application is removed THEN inform the delegate`() {
        service.delegate = mock()

        service.onTaskRemoved(null)

        verify(service.delegate)!!.onTaskRemoved()
    }

    @Test
    fun `WHEN the service is bound THEN return the current binder instance`() {
        service.binder = mock()

        val result = service.onBind(null)

        assertEquals(service.binder, result)
    }

    @Test
    fun `WHEN the play intent is asked for THEN it is set with an action to play media`() {
        val intent = AbstractMediaSessionService.playIntent(testContext, FakeMediaService::class.java)

        assertEquals(AbstractMediaSessionService.ACTION_PLAY, intent.action)
        assertEquals(ComponentName(testContext, FakeMediaService::class.java), intent.component)
    }

    @Test
    fun `WHEN the play intent is asked for THEN it is set with an action to pause media`() {
        val intent = AbstractMediaSessionService.pauseIntent(testContext, FakeMediaService::class.java)

        assertEquals(AbstractMediaSessionService.ACTION_PAUSE, intent.action)
        assertEquals(ComponentName(testContext, FakeMediaService::class.java), intent.component)
    }
}

class FakeMediaService : AbstractMediaSessionService() {
    public override val store: BrowserStore = mock()
}
