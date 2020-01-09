/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.amo

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.action.WebExtensionAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.WebExtensionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.webextension.ActionHandler
import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.AddonManager
import mozilla.components.feature.addons.AddonManagerException
import mozilla.components.feature.addons.AddonsProvider
import mozilla.components.feature.addons.update.AddonUpdater.Status
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.whenever
import mozilla.components.support.webextensions.WebExtensionSupport
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class AddonManagerTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Before
    fun setup() {
        WebExtensionSupport.installedExtensions.clear()
    }

    @After
    fun after() {
        WebExtensionSupport.installedExtensions.clear()
    }

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
                extensions = mapOf(
                    "ext1" to WebExtensionState("ext1", "url"),
                    "unsupported_ext" to WebExtensionState("unsupported_ext", "url", enabled = false)
                )
            )
        )

        WebExtensionSupport.initialize(engine, store)
        val extension: WebExtension = mock()
        whenever(extension.id).thenReturn("ext1")
        WebExtensionSupport.installedExtensions["ext1"] = extension

        val unsupportedExtension: WebExtension = mock()
        whenever(unsupportedExtension.id).thenReturn("unsupported_ext")
        whenever(unsupportedExtension.url).thenReturn("site_url")
        WebExtensionSupport.installedExtensions["unsupported_ext"] = unsupportedExtension

        // Verify add-ons were updated with state provided by the engine/store
        val addons = AddonManager(store, mock(), addonsProvider, mock()).getAddons()
        assertEquals(3, addons.size)
        assertEquals("ext1", addons[0].id)
        assertNotNull(addons[0].installedState)
        assertEquals("ext1", addons[0].installedState!!.id)

        assertEquals("ext2", addons[1].id)
        assertNull(addons[1].installedState)

        // Verify the unsupported add-on was included in addons
        assertEquals("unsupported_ext", addons[2].id)
        assertFalse(addons[2].installedState!!.supported)
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

        AddonManager(store, mock(), addonsProvider, mock()).getAddons()
        Unit
    }

    @Test
    fun `updateAddon - when a extension is updated successfully`() {
        val store = spy(
            BrowserStore(
                BrowserState(
                    tabs = listOf(
                        createTab(id = "1", url = "https://www.mozilla.org")
                    ),
                    extensions = mapOf("extensionId" to mock())
                )
            )
        )
        val engine: Engine = mock()
        val engineSession: EngineSession = mock()
        val onSuccessCaptor = argumentCaptor<((WebExtension?) -> Unit)>()
        var updateStatus: Status? = null
        val manager = AddonManager(store, engine, mock(), mock())

        val updatedExt: WebExtension = mock()
        whenever(updatedExt.id).thenReturn("extensionId")
        whenever(updatedExt.url).thenReturn("url")
        whenever(updatedExt.supportActions).thenReturn(true)

        store.dispatch(EngineAction.LinkEngineSessionAction("1", engineSession)).joinBlocking()
        WebExtensionSupport.installedExtensions["extensionId"] = mock()

        val oldExt = WebExtensionSupport.installedExtensions["extensionId"]

        manager.updateAddon("extensionId") { status ->
            updateStatus = status
        }

        val actionHandlerCaptor = argumentCaptor<ActionHandler>()
        val actionCaptor = argumentCaptor<WebExtensionAction.UpdateWebExtension>()

        // Verifying we returned the right status
        verify(engine).updateWebExtension(any(), onSuccessCaptor.capture(), any())
        onSuccessCaptor.value.invoke(updatedExt)
        assertEquals(Status.SuccessfullyUpdated, updateStatus)

        // Verifying we updated the extension in WebExtensionSupport
        assertNotEquals(oldExt, WebExtensionSupport.installedExtensions["extensionId"])
        assertEquals(updatedExt, WebExtensionSupport.installedExtensions["extensionId"])

        // Verifying we updated the extension in the store
        verify(store, times(2)).dispatch(actionCaptor.capture())
        assertEquals(
            WebExtensionState(updatedExt.id, updatedExt.url),
            actionCaptor.allValues.last().updatedExtension
        )

        // Verify that we registered an action handler for all existing sessions on the extension
        verify(updatedExt).registerActionHandler(eq(engineSession), actionHandlerCaptor.capture())
        actionHandlerCaptor.value.onBrowserAction(updatedExt, engineSession, mock())
    }

    @Test
    fun `updateAddon - when extensionId is not installed`() {
        var updateStatus: Status? = null

        val manager = AddonManager(mock(), mock(), mock(), mock())

        manager.updateAddon("extensionId") { status ->
            updateStatus = status
        }

        assertEquals(Status.NotInstalled, updateStatus)
    }

    @Test
    fun `updateAddon - when an error happens while updating`() {
        val engine: Engine = mock()
        val onErrorCaptor = argumentCaptor<((String, Throwable) -> Unit)>()
        var updateStatus: Status? = null
        val manager = AddonManager(mock(), engine, mock(), mock())

        WebExtensionSupport.installedExtensions["extensionId"] = mock()

        manager.updateAddon("extensionId") { status ->
            updateStatus = status
        }

        // Verifying we returned the right status
        verify(engine).updateWebExtension(any(), any(), onErrorCaptor.capture())
        onErrorCaptor.value.invoke("message", Exception())
        assertTrue(updateStatus is Status.Error)
    }

    @Test
    fun `updateAddon - when there is not new updates for the extension`() {
        val engine: Engine = mock()
        val onSuccessCaptor = argumentCaptor<((WebExtension?) -> Unit)>()
        var updateStatus: Status? = null
        val manager = AddonManager(mock(), engine, mock(), mock())

        WebExtensionSupport.installedExtensions["extensionId"] = mock()
        manager.updateAddon("extensionId") { status ->
            updateStatus = status
        }

        verify(engine).updateWebExtension(any(), onSuccessCaptor.capture(), any())
        onSuccessCaptor.value.invoke(null)
        assertEquals(Status.NoUpdateAvailable, updateStatus)
    }

    @Test
    fun `installAddon successfully`() {
        val addon = Addon(
            id = "ext1",
            authors = emptyList(),
            categories = emptyList(),
            downloadUrl = "",
            version = "",
            createdAt = "",
            updatedAt = ""
        )

        val engine: Engine = mock()
        val onSuccessCaptor = argumentCaptor<((WebExtension) -> Unit)>()

        var installedAddon: Addon? = null
        val manager = AddonManager(mock(), engine, mock(), mock())
        manager.installAddon(addon, onSuccess = {
            installedAddon = it
        })

        verify(engine).installWebExtension(
            eq("ext1"), any(), anyBoolean(), anyBoolean(), onSuccessCaptor.capture(), any()
        )

        val extension: WebExtension = mock()
        whenever(extension.id).thenReturn("ext1")
        onSuccessCaptor.value.invoke(extension)
        assertNotNull(installedAddon)
        assertEquals(addon.id, installedAddon!!.id)
    }

    @Test
    fun `installAddon failure`() {
        val addon = Addon(id = "ext1",
            authors = emptyList(),
            categories = emptyList(),
            downloadUrl = "",
            version = "",
            createdAt = "",
            updatedAt = ""
        )

        val engine: Engine = mock()
        val onErrorCaptor = argumentCaptor<((String, Throwable) -> Unit)>()

        var throwable: Throwable? = null
        var msg: String? = null
        val manager = AddonManager(mock(), engine, mock(), mock())
        manager.installAddon(addon, onError = { errorMsg, caught ->
            throwable = caught
            msg = errorMsg
        })

        verify(engine).installWebExtension(
            eq("ext1"), any(), anyBoolean(), anyBoolean(), any(), onErrorCaptor.capture()
        )

        onErrorCaptor.value.invoke(addon.id, IllegalStateException("test"))
        assertNotNull(throwable!!)
        assertEquals(msg, addon.id)
    }

    @Test
    fun `uninstallAddon successfully`() {
        val installedAddon = Addon(
            id = "ext1",
            authors = emptyList(),
            categories = emptyList(),
            downloadUrl = "",
            version = "",
            createdAt = "",
            updatedAt = "",
            installedState = Addon.InstalledState("ext1", "1.0", "", true)
        )

        val extension: WebExtension = mock()
        whenever(extension.id).thenReturn("ext1")
        WebExtensionSupport.installedExtensions[installedAddon.id] = extension

        val engine: Engine = mock()
        val onSuccessCaptor = argumentCaptor<(() -> Unit)>()

        var successCallbackInvoked = false
        val manager = AddonManager(mock(), engine, mock(), mock())
        manager.uninstallAddon(installedAddon, onSuccess = {
            successCallbackInvoked = true
        })
        verify(engine).uninstallWebExtension(eq(extension), onSuccessCaptor.capture(), any())

        onSuccessCaptor.value.invoke()
        assertTrue(successCallbackInvoked)
    }

    @Test
    fun `uninstallAddon failure cases`() {
        val addon = Addon(
            id = "ext1",
            authors = emptyList(),
            categories = emptyList(),
            downloadUrl = "",
            version = "",
            createdAt = "",
            updatedAt = ""
        )

        val engine: Engine = mock()
        val onErrorCaptor = argumentCaptor<((String, Throwable) -> Unit)>()
        var throwable: Throwable? = null
        var msg: String? = null
        val errorCallback = { errorMsg: String, caught: Throwable ->
            throwable = caught
            msg = errorMsg
        }
        val manager = AddonManager(mock(), engine, mock(), mock())

        // Extension is not installed so we're invoking the error callback and never the engine
        manager.uninstallAddon(addon, onError = errorCallback)
        verify(engine, never()).uninstallWebExtension(any(), any(), onErrorCaptor.capture())
        assertNotNull(throwable!!)
        assertEquals("Addon is not installed", throwable!!.localizedMessage)

        // Install extension and try again
        val extension: WebExtension = mock()
        whenever(extension.id).thenReturn("ext1")
        WebExtensionSupport.installedExtensions[addon.id] = extension
        manager.uninstallAddon(addon, onError = errorCallback)
        verify(engine, never()).uninstallWebExtension(any(), any(), onErrorCaptor.capture())

        // Make sure engine error is forwarded to caller
        val installedAddon = addon.copy(installedState = Addon.InstalledState(addon.id, "1.0", "", true))
        manager.uninstallAddon(installedAddon, onError = errorCallback)
        verify(engine).uninstallWebExtension(eq(extension), any(), onErrorCaptor.capture())
        onErrorCaptor.value.invoke(addon.id, IllegalStateException("test"))
        assertNotNull(throwable!!)
        assertEquals("test", throwable!!.localizedMessage)
        assertEquals(msg, addon.id)
    }

    @Test
    fun `enableAddon successfully`() {
        val addon = Addon(
            id = "ext1",
            authors = emptyList(),
            categories = emptyList(),
            downloadUrl = "",
            version = "",
            createdAt = "",
            updatedAt = "",
            installedState = Addon.InstalledState("ext1", "1.0", "", true)
        )

        val extension: WebExtension = mock()
        whenever(extension.id).thenReturn("ext1")
        WebExtensionSupport.installedExtensions[addon.id] = extension

        val engine: Engine = mock()
        val onSuccessCaptor = argumentCaptor<((WebExtension) -> Unit)>()

        var enabledAddon: Addon? = null
        val manager = AddonManager(mock(), engine, mock(), mock())
        manager.enableAddon(addon, onSuccess = {
            enabledAddon = it
        })

        verify(engine).enableWebExtension(eq(extension), onSuccessCaptor.capture(), any())
        onSuccessCaptor.value.invoke(extension)
        assertNotNull(enabledAddon)
        assertEquals(addon.id, enabledAddon!!.id)
    }

    @Test
    fun `enableAddon failure cases`() {
        val addon = Addon(
            id = "ext1",
            authors = emptyList(),
            categories = emptyList(),
            downloadUrl = "",
            version = "",
            createdAt = "",
            updatedAt = ""
        )
        val engine: Engine = mock()
        val onErrorCaptor = argumentCaptor<((Throwable) -> Unit)>()
        var throwable: Throwable? = null
        val errorCallback = { caught: Throwable ->
            throwable = caught
        }
        val manager = AddonManager(mock(), engine, mock(), mock())

        // Extension is not installed so we're invoking the error callback and never the engine
        manager.enableAddon(addon, onError = errorCallback)
        verify(engine, never()).enableWebExtension(any(), any(), onErrorCaptor.capture())
        assertNotNull(throwable!!)
        assertEquals("Addon is not installed", throwable!!.localizedMessage)

        // Install extension and try again
        val extension: WebExtension = mock()
        whenever(extension.id).thenReturn("ext1")
        WebExtensionSupport.installedExtensions[addon.id] = extension
        manager.enableAddon(addon, onError = errorCallback)
        verify(engine, never()).enableWebExtension(any(), any(), onErrorCaptor.capture())

        // Make sure engine error is forwarded to caller
        val installedAddon = addon.copy(installedState = Addon.InstalledState(addon.id, "1.0", "", true))
        manager.enableAddon(installedAddon, onError = errorCallback)
        verify(engine).enableWebExtension(eq(extension), any(), onErrorCaptor.capture())
        onErrorCaptor.value.invoke(IllegalStateException("test"))
        assertNotNull(throwable!!)
        assertEquals("test", throwable!!.localizedMessage)
    }

    @Test
    fun `disableAddon successfully`() {
        val addon = Addon(
            id = "ext1",
            authors = emptyList(),
            categories = emptyList(),
            downloadUrl = "",
            version = "",
            createdAt = "",
            updatedAt = "",
            installedState = Addon.InstalledState("ext1", "1.0", "", true)
        )

        val extension: WebExtension = mock()
        whenever(extension.id).thenReturn("ext1")
        WebExtensionSupport.installedExtensions[addon.id] = extension

        val engine: Engine = mock()
        val onSuccessCaptor = argumentCaptor<((WebExtension) -> Unit)>()

        var disabledAddon: Addon? = null
        val manager = AddonManager(mock(), engine, mock(), mock())
        manager.disableAddon(addon, onSuccess = {
            disabledAddon = it
        })

        verify(engine).disableWebExtension(eq(extension), onSuccessCaptor.capture(), any())
        onSuccessCaptor.value.invoke(extension)
        assertNotNull(disabledAddon)
        assertEquals(addon.id, disabledAddon!!.id)
    }

    @Test
    fun `disableAddon failure cases`() {
        val addon = Addon(
            id = "ext1",
            authors = emptyList(),
            categories = emptyList(),
            downloadUrl = "",
            version = "",
            createdAt = "",
            updatedAt = ""
        )
        val engine: Engine = mock()
        val onErrorCaptor = argumentCaptor<((Throwable) -> Unit)>()
        var throwable: Throwable? = null
        val errorCallback = { caught: Throwable ->
            throwable = caught
        }
        val manager = AddonManager(mock(), engine, mock(), mock())

        // Extension is not installed so we're invoking the error callback and never the engine
        manager.disableAddon(addon, onError = errorCallback)
        verify(engine, never()).disableWebExtension(any(), any(), onErrorCaptor.capture())
        assertNotNull(throwable!!)
        assertEquals("Addon is not installed", throwable!!.localizedMessage)

        // Install extension and try again
        val extension: WebExtension = mock()
        whenever(extension.id).thenReturn("ext1")
        WebExtensionSupport.installedExtensions[addon.id] = extension
        manager.disableAddon(addon, onError = errorCallback)
        verify(engine, never()).disableWebExtension(any(), any(), onErrorCaptor.capture())

        // Make sure engine error is forwarded to caller
        val installedAddon = addon.copy(installedState = Addon.InstalledState(addon.id, "1.0", "", true))
        manager.disableAddon(installedAddon, onError = errorCallback)
        verify(engine).disableWebExtension(eq(extension), any(), onErrorCaptor.capture())
        onErrorCaptor.value.invoke(IllegalStateException("test"))
        assertNotNull(throwable!!)
        assertEquals("test", throwable!!.localizedMessage)
    }
}
