/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.amo

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.WebExtensionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.AddonManager
import mozilla.components.feature.addons.AddonManagerException
import mozilla.components.feature.addons.AddonsProvider
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import mozilla.components.support.webextensions.WebExtensionSupport
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyBoolean
import java.lang.IllegalStateException

@RunWith(AndroidJUnit4::class)
class AddonManagerTest {

    @Test
    fun `getAddons - queries addons from provider and updates installation state`() = runBlocking {
        // Prepare addons provider
        val addon1 = Addon(
            id = "ext1",
            authors = emptyList(),
            categories = emptyList(),
            downloadUrl = "",
            version = "",
            createdAt = "",
            updatedAt = ""
        )
        val addon2 = Addon(
            id = "ext2",
            authors = emptyList(),
            categories = emptyList(),
            downloadUrl = "",
            version = "",
            createdAt = "",
            updatedAt = ""
        )
        val addonsProvider: AddonsProvider = mock()
        whenever(addonsProvider.getAvailableAddons(anyBoolean())).thenReturn(listOf(addon1, addon2))

        // Prepare engine
        val engine: Engine = mock()
        val callbackCaptor = argumentCaptor<((List<WebExtension>) -> Unit)>()
        whenever(engine.listInstalledWebExtensions(callbackCaptor.capture(), any())).thenAnswer {
            callbackCaptor.value.invoke(emptyList())
        }
        val store = BrowserStore(
            BrowserState(
                extensions = mapOf("ext1" to WebExtensionState("ext1", "url"))
            )
        )
        WebExtensionSupport.initialize(engine, store)

        // Verify add-ons were updated with state provided by the engine/store
        val addons = AddonManager(store, addonsProvider).getAddons()
        assertEquals(2, addons.size)
        assertEquals("ext1", addons[0].id)
        assertNotNull(addons[0].installedState)
        assertEquals("ext1", addons[0].installedState!!.id)

        assertEquals("ext2", addons[1].id)
        assertNull(addons[1].installedState)
    }

    @Test(expected = AddonManagerException::class)
    fun `getAddons - wraps exceptions and rethrows them`() = runBlocking {
        val store = BrowserStore()

        val engine: Engine = mock()
        val callbackCaptor = argumentCaptor<((List<WebExtension>) -> Unit)>()
        whenever(engine.listInstalledWebExtensions(callbackCaptor.capture(), any())).thenAnswer {
            callbackCaptor.value.invoke(emptyList())
        }

        val addonsProvider: AddonsProvider = mock()
        whenever(addonsProvider.getAvailableAddons(anyBoolean())).thenThrow(IllegalStateException("test"))
        WebExtensionSupport.initialize(engine, store)

        AddonManager(store, addonsProvider).getAddons()
        Unit
    }
}
