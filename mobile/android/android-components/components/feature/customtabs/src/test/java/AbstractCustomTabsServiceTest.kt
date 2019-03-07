/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.tab

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.os.IBinder
import android.support.customtabs.ICustomTabsCallback
import android.support.customtabs.ICustomTabsService
import androidx.browser.customtabs.CustomTabsService
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.customtabs.AbstractCustomTabsService
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.mock
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class AbstractCustomTabsServiceTest {

    @Test
    fun customTabService() {
        val customTabsService = object : AbstractCustomTabsService() {
            override val engine: Engine
                get() = mock()
        }

        val customTabsServiceStub = customTabsService.onBind(mock(Intent::class.java))
        assertNotNull(customTabsServiceStub)

        val stub = customTabsServiceStub as ICustomTabsService.Stub

        val callback = mock(ICustomTabsCallback::class.java)
        doReturn(mock<IBinder>()).`when`(callback).asBinder()

        assertTrue(stub.warmup(123))
        assertTrue(stub.newSession(callback))
        assertNull(stub.extraCommand("", mock(Bundle::class.java)))
        assertFalse(stub.updateVisuals(mock(ICustomTabsCallback::class.java), mock(Bundle::class.java)))
        assertFalse(stub.requestPostMessageChannel(mock(ICustomTabsCallback::class.java), mock(Uri::class.java)))
        assertEquals(CustomTabsService.RESULT_FAILURE_DISALLOWED,
            stub.postMessage(mock(ICustomTabsCallback::class.java), "", mock(Bundle::class.java)))
        assertFalse(stub.validateRelationship(
            mock(ICustomTabsCallback::class.java),
            0,
            mock(Uri::class.java),
            mock(Bundle::class.java)))
        assertTrue(stub.mayLaunchUrl(
            mock(ICustomTabsCallback::class.java),
            mock(Uri::class.java),
            mock(Bundle::class.java), emptyList<Bundle>()))
    }

    @Test
    fun `Warmup will access engine instance`() {
        var engineAccessed = false

        val customTabsService = object : AbstractCustomTabsService() {
            override val engine: Engine
                get() {
                    engineAccessed = true
                    return mock()
                }
        }

        val stub = customTabsService.onBind(mock(Intent::class.java)) as ICustomTabsService.Stub

        assertTrue(stub.warmup(42))

        assertTrue(engineAccessed)
    }

    @Test
    fun `mayLaunchUrl opens a speculative connection for most likely URL`() {
        val engine: Engine = mock()

        val customTabsService = object : AbstractCustomTabsService() {
            override val engine: Engine = engine
        }

        val stub = customTabsService.onBind(mock(Intent::class.java)) as ICustomTabsService.Stub

        assertTrue(stub.mayLaunchUrl(mock(), Uri.parse("https://www.mozilla.org"), Bundle(), listOf()))

        verify(engine).speculativeConnect("https://www.mozilla.org")
    }
}
