/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.customtabs

import android.content.pm.PackageManager
import android.net.Uri
import android.os.Bundle
import android.os.IBinder
import android.support.customtabs.ICustomTabsCallback
import android.support.customtabs.ICustomTabsService
import androidx.browser.customtabs.CustomTabsService
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.fetch.Client
import mozilla.components.feature.customtabs.store.CustomTabsServiceStore
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class AbstractCustomTabsServiceTest {

    @Test
    fun customTabService() {
        val customTabsService = object : AbstractCustomTabsService() {
            override val engine: Engine
                get() = mock()

            override fun getPackageManager(): PackageManager = mock()
        }

        val customTabsServiceStub = customTabsService.onBind(mock())
        assertNotNull(customTabsServiceStub)

        val stub = customTabsServiceStub as ICustomTabsService.Stub

        val callback = mock<ICustomTabsCallback>()
        doReturn(mock<IBinder>()).`when`(callback).asBinder()

        assertTrue(stub.warmup(123))
        assertTrue(stub.newSession(callback))
        assertNull(stub.extraCommand("", mock()))
        assertFalse(stub.updateVisuals(mock(), mock()))
        assertFalse(stub.requestPostMessageChannel(mock(), mock()))
        assertEquals(CustomTabsService.RESULT_FAILURE_DISALLOWED,
            stub.postMessage(mock(), "", mock()))
        assertFalse(stub.validateRelationship(
            mock(),
            0,
            mock(),
            mock()))
        assertTrue(stub.mayLaunchUrl(
            mock(),
            mock(),
            mock(), emptyList<Bundle>()))
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

        val stub = customTabsService.onBind(mock()) as ICustomTabsService.Stub

        assertTrue(stub.warmup(42))

        assertTrue(engineAccessed)
    }

    @Test
    fun `mayLaunchUrl opens a speculative connection for most likely URL`() {
        val engine: Engine = mock()

        val customTabsService = object : AbstractCustomTabsService() {
            override val engine: Engine = engine
        }

        val stub = customTabsService.onBind(mock()) as ICustomTabsService.Stub

        assertTrue(stub.mayLaunchUrl(mock(), Uri.parse("https://www.mozilla.org"), Bundle(), listOf()))

        verify(engine).speculativeConnect("https://www.mozilla.org")
    }

    @Test
    fun `verifier is only created when store and client are provided`() {
        val justEngine = object : AbstractCustomTabsService() {
            override val engine: Engine = mock()
        }
        assertNull(justEngine.verifier)

        val withStore = object : AbstractCustomTabsService() {
            override val engine: Engine = mock()
            override val customTabsServiceStore: CustomTabsServiceStore? = mock()
        }
        assertNull(withStore.verifier)

        val withClient = object : AbstractCustomTabsService() {
            override val engine: Engine = mock()
            override val httpClient: Client? = mock()
        }
        assertNull(withClient.verifier)

        val both = object : AbstractCustomTabsService() {
            override val engine: Engine = mock()
            override val customTabsServiceStore: CustomTabsServiceStore? = mock()
            override val httpClient: Client? = mock()

            override fun getPackageManager(): PackageManager = mock()
        }
        assertNotNull(both.verifier)
    }

    @Test
    fun `API key is passed to verifier`() {
        val service = object : AbstractCustomTabsService() {
            override val engine: Engine = mock()
            override val customTabsServiceStore: CustomTabsServiceStore? = mock()
            override val httpClient: Client? = mock()
            override val apiKey: String? = "a-completely-valid-key"

            override fun getPackageManager(): PackageManager = mock()
        }
        assertEquals("a-completely-valid-key", service.verifier?.apiKey)
    }
}
