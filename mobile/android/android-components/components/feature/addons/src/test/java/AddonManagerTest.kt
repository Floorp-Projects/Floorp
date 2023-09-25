/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons

import android.graphics.Bitmap
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.launch
import mozilla.components.browser.state.action.WebExtensionAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.WebExtensionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.webextension.ActionHandler
import mozilla.components.concept.engine.webextension.DisabledFlags
import mozilla.components.concept.engine.webextension.DisabledFlags.Companion.APP_SUPPORT
import mozilla.components.concept.engine.webextension.DisabledFlags.Companion.BLOCKLIST
import mozilla.components.concept.engine.webextension.DisabledFlags.Companion.USER
import mozilla.components.concept.engine.webextension.EnableSource
import mozilla.components.concept.engine.webextension.Metadata
import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.feature.addons.AddonManager.Companion.TEMPORARY_ADDON_ICON_SIZE
import mozilla.components.feature.addons.update.AddonUpdater.Status
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
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
import org.mockito.ArgumentMatchers.anyLong
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
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
    fun `getAddons - queries addons from provider and updates installation state`() = runTestOnMain {
        // Prepare addons provider
        // addon1 (ext1) is a featured extension that is already installed.
        val addon1 = Addon(id = "ext1")
        // addon2 (ext2) is a featured extension that is not installed.
        val addon2 = Addon(id = "ext2")
        // addon3 (ext3) is a featured extension that is marked as disabled.
        val addon3 = Addon(id = "ext3")
        // addon4 (ext4) and addon5 (ext5) are not featured extensions but they are installed.
        val addon4 = Addon(id = "ext4")
        val addon5 = Addon(id = "ext5")
        val addonsProvider: AddonsProvider = mock()
        whenever(addonsProvider.getFeaturedAddons(anyBoolean(), eq(null), language = anyString())).thenReturn(listOf(addon1, addon2, addon3))
        // addon4 (ext4) and addon5 (ext5) should already be installed, which is why we expect a call
        // to `getAddonsByGUIDs()` with a list of two GUIDs.
        whenever(addonsProvider.getAddonsByGUIDs(eq(listOf("ext5", "ext4")), anyBoolean(), eq(null), language = anyString())).thenReturn(listOf(addon4, addon5))

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
                    "ext4" to WebExtensionState("ext4", "url"),
                    "ext5" to WebExtensionState("ext5", "url"),
                    // ext6 is a temporarily loaded extension.
                    "ext6" to WebExtensionState("ext6", "url"),
                    // ext7 is a built-in extension.
                    "ext7" to WebExtensionState("ext7", "url"),
                ),
            ),
        )

        WebExtensionSupport.initialize(engine, store)
        val ext1: WebExtension = mock()
        whenever(ext1.id).thenReturn("ext1")
        whenever(ext1.isEnabled()).thenReturn(true)
        WebExtensionSupport.installedExtensions["ext1"] = ext1

        // Make `ext3` an extension that is disabled because it wasn't supported.
        val newlySupportedExtension: WebExtension = mock()
        val metadata: Metadata = mock()
        whenever(newlySupportedExtension.isEnabled()).thenReturn(false)
        whenever(metadata.disabledFlags).thenReturn(DisabledFlags.select(DisabledFlags.APP_SUPPORT))
        whenever(metadata.optionsPageUrl).thenReturn("http://options-page.moz")
        whenever(metadata.openOptionsPageInTab).thenReturn(true)
        whenever(newlySupportedExtension.id).thenReturn("ext3")
        whenever(newlySupportedExtension.url).thenReturn("site_url")
        whenever(newlySupportedExtension.getMetadata()).thenReturn(metadata)
        WebExtensionSupport.installedExtensions["ext3"] = newlySupportedExtension

        val ext4: WebExtension = mock()
        whenever(ext4.id).thenReturn("ext4")
        whenever(ext4.isEnabled()).thenReturn(true)
        val ext4Metadata: Metadata = mock()
        whenever(ext4Metadata.temporary).thenReturn(false)
        whenever(ext4.getMetadata()).thenReturn(ext4Metadata)
        WebExtensionSupport.installedExtensions["ext4"] = ext4

        val ext5: WebExtension = mock()
        whenever(ext5.id).thenReturn("ext5")
        whenever(ext5.isEnabled()).thenReturn(true)
        val ext5Metadata: Metadata = mock()
        whenever(ext5Metadata.temporary).thenReturn(false)
        whenever(ext5.getMetadata()).thenReturn(ext5Metadata)
        WebExtensionSupport.installedExtensions["ext5"] = ext5

        val ext6: WebExtension = mock()
        whenever(ext6.id).thenReturn("ext6")
        whenever(ext6.url).thenReturn("some url")
        whenever(ext6.isEnabled()).thenReturn(true)
        val ext6Metadata: Metadata = mock()
        whenever(ext6Metadata.name).thenReturn("temporarily loaded extension - ext6")
        whenever(ext6Metadata.temporary).thenReturn(true)
        whenever(ext6.getMetadata()).thenReturn(ext6Metadata)
        val ext6Icon: Bitmap = mock()
        whenever(ext6.loadIcon(TEMPORARY_ADDON_ICON_SIZE)).thenReturn(ext6Icon)
        WebExtensionSupport.installedExtensions["ext6"] = ext6

        val ext7: WebExtension = mock()
        whenever(ext7.id).thenReturn("ext7")
        whenever(ext7.isEnabled()).thenReturn(true)
        whenever(ext7.isBuiltIn()).thenReturn(true)
        WebExtensionSupport.installedExtensions["ext7"] = ext7

        // Verify add-ons were updated with state provided by the engine/store.
        val addons = AddonManager(store, mock(), addonsProvider, mock()).getAddons()
        assertEquals(6, addons.size)
        // ext1 should be installed.
        assertEquals("ext1", addons[0].id)
        assertNotNull(addons[0].installedState)
        assertEquals("ext1", addons[0].installedState!!.id)
        assertTrue(addons[0].isEnabled())
        assertFalse(addons[0].isDisabledAsUnsupported())
        assertNull(addons[0].installedState!!.optionsPageUrl)
        assertFalse(addons[0].installedState!!.openOptionsPageInTab)
        // ext2 should not be installed.
        assertEquals("ext2", addons[1].id)
        assertNull(addons[1].installedState)
        // ext3 should now be marked as supported but still be disabled as unsupported.
        assertEquals("ext3", addons[2].id)
        assertNotNull(addons[2].installedState)
        assertEquals("ext3", addons[2].installedState!!.id)
        assertTrue(addons[2].isSupported())
        assertFalse(addons[2].isEnabled())
        assertTrue(addons[2].isDisabledAsUnsupported())
        assertEquals("http://options-page.moz", addons[2].installedState!!.optionsPageUrl)
        assertTrue(addons[2].installedState!!.openOptionsPageInTab)
        // ext4 should be installed.
        assertEquals("ext4", addons[3].id)
        assertNotNull(addons[3].installedState)
        assertEquals("ext4", addons[3].installedState!!.id)
        assertTrue(addons[3].isEnabled())
        assertFalse(addons[3].isDisabledAsUnsupported())
        assertNull(addons[3].installedState!!.optionsPageUrl)
        assertFalse(addons[3].installedState!!.openOptionsPageInTab)
        // ext5 should be installed.
        assertEquals("ext5", addons[4].id)
        assertNotNull(addons[4].installedState)
        assertEquals("ext5", addons[4].installedState!!.id)
        assertTrue(addons[4].isEnabled())
        assertFalse(addons[4].isDisabledAsUnsupported())
        assertNull(addons[4].installedState!!.optionsPageUrl)
        assertFalse(addons[4].installedState!!.openOptionsPageInTab)
        // ext6 should be installed.
        assertEquals("ext6", addons[5].id)
        assertNotNull(addons[5].installedState)
        assertEquals("ext6", addons[5].installedState!!.id)
        assertTrue(addons[5].isEnabled())
        assertFalse(addons[5].isDisabledAsUnsupported())
        assertNull(addons[5].installedState!!.optionsPageUrl)
        assertFalse(addons[5].installedState!!.openOptionsPageInTab)
    }

    @Test
    fun `getAddons - returns temporary add-ons as supported`() = runTestOnMain {
        val addonsProvider: AddonsProvider = mock()
        whenever(addonsProvider.getFeaturedAddons(anyBoolean(), eq(null), language = anyString())).thenReturn(listOf())
        whenever(addonsProvider.getAddonsByGUIDs(any(), anyBoolean(), eq(null), language = anyString())).thenReturn(emptyList())

        // Prepare engine
        val engine: Engine = mock()
        val callbackCaptor = argumentCaptor<((List<WebExtension>) -> Unit)>()
        whenever(engine.listInstalledWebExtensions(callbackCaptor.capture(), any())).thenAnswer {
            callbackCaptor.value.invoke(emptyList())
        }

        val store = BrowserStore()
        WebExtensionSupport.initialize(engine, store)

        // Add temporary extension
        val temporaryExtension: WebExtension = mock()
        val temporaryExtensionIcon: Bitmap = mock()
        val temporaryExtensionMetadata: Metadata = mock()
        whenever(temporaryExtensionMetadata.temporary).thenReturn(true)
        whenever(temporaryExtensionMetadata.name).thenReturn("name")
        whenever(temporaryExtension.id).thenReturn("temp_ext")
        whenever(temporaryExtension.url).thenReturn("site_url")
        whenever(temporaryExtension.getMetadata()).thenReturn(temporaryExtensionMetadata)
        whenever(temporaryExtension.loadIcon(TEMPORARY_ADDON_ICON_SIZE)).thenReturn(temporaryExtensionIcon)
        WebExtensionSupport.installedExtensions["temp_ext"] = temporaryExtension

        val addonManager = spy(AddonManager(store, mock(), addonsProvider, mock()))
        whenever(addonManager.getIconDispatcher()).thenReturn(Dispatchers.Main)

        val addons = addonManager.getAddons()
        assertEquals(1, addons.size)

        // Temporary extension should be returned and marked as supported
        assertEquals("temp_ext", addons[0].id)
        assertEquals(1, addons[0].translatableName.size)
        assertNotNull(addons[0].translatableName[addons[0].defaultLocale])
        assertTrue(addons[0].translatableName.containsValue("name"))
        assertNotNull(addons[0].installedState)
        assertTrue(addons[0].isSupported())
        assertEquals(temporaryExtensionIcon, addons[0].installedState!!.icon)
    }

    @Test(expected = AddonManagerException::class)
    fun `getAddons - wraps exceptions and rethrows them`() = runTestOnMain {
        val store = BrowserStore()

        val engine: Engine = mock()
        val callbackCaptor = argumentCaptor<((List<WebExtension>) -> Unit)>()
        whenever(engine.listInstalledWebExtensions(callbackCaptor.capture(), any())).thenAnswer {
            callbackCaptor.value.invoke(emptyList())
        }

        val addonsProvider: AddonsProvider = mock()
        whenever(addonsProvider.getFeaturedAddons(anyBoolean(), anyLong(), language = anyString())).thenThrow(IllegalStateException("test"))
        WebExtensionSupport.initialize(engine, store)

        AddonManager(store, mock(), addonsProvider, mock()).getAddons()
        Unit
    }

    @Test
    fun `getAddons - filters unneeded locales on featured add-ons`() = runTestOnMain {
        val addon = Addon(
            id = "addon1",
            translatableName = mapOf(Addon.DEFAULT_LOCALE to "name", "invalid1" to "Name", "invalid2" to "nombre"),
            translatableDescription = mapOf(Addon.DEFAULT_LOCALE to "description", "invalid1" to "Beschreibung", "invalid2" to "descripción"),
            translatableSummary = mapOf(Addon.DEFAULT_LOCALE to "summary", "invalid1" to "Kurzfassung", "invalid2" to "resumen"),
        )

        val store = BrowserStore()

        val engine: Engine = mock()
        val callbackCaptor = argumentCaptor<((List<WebExtension>) -> Unit)>()
        whenever(engine.listInstalledWebExtensions(callbackCaptor.capture(), any())).thenAnswer {
            callbackCaptor.value.invoke(emptyList())
        }

        val addonsProvider: AddonsProvider = mock()
        whenever(addonsProvider.getFeaturedAddons(anyBoolean(), eq(null), language = anyString())).thenReturn(listOf(addon))
        whenever(addonsProvider.getAddonsByGUIDs(any(), anyBoolean(), eq(null), language = anyString())).thenReturn(emptyList())
        WebExtensionSupport.initialize(engine, store)

        val addons = AddonManager(store, mock(), addonsProvider, mock()).getAddons()
        assertEquals(1, addons[0].translatableName.size)
        assertTrue(addons[0].translatableName.contains(addons[0].defaultLocale))
        assertEquals(1, addons[0].translatableDescription.size)
        assertTrue(addons[0].translatableDescription.contains(addons[0].defaultLocale))
        assertEquals(1, addons[0].translatableSummary.size)
        assertTrue(addons[0].translatableSummary.contains(addons[0].defaultLocale))
    }

    @Test
    fun `getAddons - filters unneeded locales on non-featured installed add-ons`() = runTestOnMain {
        val addon = Addon(
            id = "addon1",
            translatableName = mapOf(Addon.DEFAULT_LOCALE to "name", "invalid1" to "Name", "invalid2" to "nombre"),
            translatableDescription = mapOf(Addon.DEFAULT_LOCALE to "description", "invalid1" to "Beschreibung", "invalid2" to "descripción"),
            translatableSummary = mapOf(Addon.DEFAULT_LOCALE to "summary", "invalid1" to "Kurzfassung", "invalid2" to "resumen"),
        )

        val store = BrowserStore()

        val engine: Engine = mock()
        val callbackCaptor = argumentCaptor<((List<WebExtension>) -> Unit)>()
        whenever(engine.listInstalledWebExtensions(callbackCaptor.capture(), any())).thenAnswer {
            callbackCaptor.value.invoke(emptyList())
        }

        val addonsProvider: AddonsProvider = mock()
        whenever(addonsProvider.getFeaturedAddons(anyBoolean(), eq(null), language = anyString())).thenReturn(emptyList())
        whenever(addonsProvider.getAddonsByGUIDs(eq(listOf("addon1")), anyBoolean(), eq(null), language = anyString())).thenReturn(listOf(addon))
        WebExtensionSupport.initialize(engine, store)
        val extension: WebExtension = mock()
        whenever(extension.id).thenReturn(addon.id)
        whenever(extension.isEnabled()).thenReturn(true)
        whenever(extension.getMetadata()).thenReturn(mock())
        WebExtensionSupport.installedExtensions[addon.id] = extension

        val addons = AddonManager(store, mock(), addonsProvider, mock()).getAddons()
        assertEquals(1, addons[0].translatableName.size)
        assertTrue(addons[0].translatableName.contains(addons[0].defaultLocale))
        assertEquals(1, addons[0].translatableDescription.size)
        assertTrue(addons[0].translatableDescription.contains(addons[0].defaultLocale))
        assertEquals(1, addons[0].translatableSummary.size)
        assertTrue(addons[0].translatableSummary.contains(addons[0].defaultLocale))
    }

    @Test
    fun `getAddons - suspends until pending actions are completed`() = runTestOnMain {
        val addon = Addon(
            id = "ext1",
            installedState = Addon.InstalledState("ext1", "1.0", "", true),
        )

        val extension: WebExtension = mock()
        whenever(extension.id).thenReturn("ext1")

        val store = BrowserStore()

        val engine: Engine = mock()
        val callbackCaptor = argumentCaptor<((List<WebExtension>) -> Unit)>()
        whenever(engine.listInstalledWebExtensions(callbackCaptor.capture(), any())).thenAnswer {
            callbackCaptor.value.invoke(emptyList())
        }
        val addonsProvider: AddonsProvider = mock()

        whenever(addonsProvider.getFeaturedAddons(anyBoolean(), eq(null), language = anyString())).thenReturn(listOf(addon))
        whenever(addonsProvider.getAddonsByGUIDs(any(), anyBoolean(), eq(null), language = anyString())).thenReturn(emptyList())
        WebExtensionSupport.initialize(engine, store)
        WebExtensionSupport.installedExtensions[addon.id] = extension

        val addonManager = AddonManager(store, mock(), addonsProvider, mock())
        addonManager.installAddon(addon)
        addonManager.enableAddon(addon)
        addonManager.disableAddon(addon)
        addonManager.uninstallAddon(addon)
        assertEquals(4, addonManager.pendingAddonActions.size)

        var getAddonsResult: List<Addon>? = null
        val nonSuspendingJob = CoroutineScope(Dispatchers.IO).launch {
            getAddonsResult = addonManager.getAddons(waitForPendingActions = false)
        }

        nonSuspendingJob.join()
        assertNotNull(getAddonsResult)

        getAddonsResult = null
        val suspendingJob = CoroutineScope(Dispatchers.IO).launch {
            getAddonsResult = addonManager.getAddons(waitForPendingActions = true)
        }

        addonManager.pendingAddonActions.forEach { it.complete(Unit) }

        suspendingJob.join()
        assertNotNull(getAddonsResult)
    }

    @Test
    fun `getAddons - passes on allowCache parameter`() = runTestOnMain {
        val store = BrowserStore()

        val engine: Engine = mock()
        val callbackCaptor = argumentCaptor<((List<WebExtension>) -> Unit)>()
        whenever(engine.listInstalledWebExtensions(callbackCaptor.capture(), any())).thenAnswer {
            callbackCaptor.value.invoke(emptyList())
        }
        WebExtensionSupport.initialize(engine, store)

        val addonsProvider: AddonsProvider = mock()
        whenever(addonsProvider.getFeaturedAddons(anyBoolean(), eq(null), language = anyString())).thenReturn(emptyList())
        whenever(addonsProvider.getAddonsByGUIDs(any(), anyBoolean(), eq(null), language = anyString())).thenReturn(emptyList())
        val addonsManager = AddonManager(store, mock(), addonsProvider, mock())

        addonsManager.getAddons()
        verify(addonsProvider).getFeaturedAddons(eq(true), eq(null), language = anyString())

        addonsManager.getAddons(allowCache = false)
        verify(addonsProvider).getFeaturedAddons(eq(false), eq(null), language = anyString())
        Unit
    }

    @Test
    fun `updateAddon - when a extension is updated successfully`() {
        val engine: Engine = mock()
        val engineSession: EngineSession = mock()
        val store = spy(
            BrowserStore(
                BrowserState(
                    tabs = listOf(
                        createTab(id = "1", url = "https://www.mozilla.org", engineSession = engineSession),
                    ),
                    extensions = mapOf("extensionId" to mock()),
                ),
            ),
        )
        val onSuccessCaptor = argumentCaptor<((WebExtension?) -> Unit)>()
        var updateStatus: Status? = null
        val manager = AddonManager(store, engine, mock(), mock())

        val updatedExt: WebExtension = mock()
        whenever(updatedExt.id).thenReturn("extensionId")
        whenever(updatedExt.url).thenReturn("url")
        whenever(updatedExt.supportActions).thenReturn(true)

        WebExtensionSupport.installedExtensions["extensionId"] = mock()

        val oldExt = WebExtensionSupport.installedExtensions["extensionId"]

        manager.updateAddon("extensionId") { status ->
            updateStatus = status
        }

        val actionHandlerCaptor = argumentCaptor<ActionHandler>()
        val actionCaptor = argumentCaptor<WebExtensionAction.UpdateWebExtensionAction>()

        // Verifying we returned the right status
        verify(engine).updateWebExtension(any(), onSuccessCaptor.capture(), any())
        onSuccessCaptor.value.invoke(updatedExt)
        assertEquals(Status.SuccessfullyUpdated, updateStatus)

        // Verifying we updated the extension in WebExtensionSupport
        assertNotEquals(oldExt, WebExtensionSupport.installedExtensions["extensionId"])
        assertEquals(updatedExt, WebExtensionSupport.installedExtensions["extensionId"])

        // Verifying we updated the extension in the store
        verify(store).dispatch(actionCaptor.capture())
        assertEquals(
            WebExtensionState(updatedExt.id, updatedExt.url, updatedExt.getMetadata()?.name, updatedExt.isEnabled()),
            actionCaptor.allValues.last().updatedExtension,
        )

        // Verify that we registered an action handler for all existing sessions on the extension
        verify(updatedExt).registerActionHandler(eq(engineSession), actionHandlerCaptor.capture())
        actionHandlerCaptor.value.onBrowserAction(updatedExt, engineSession, mock())
    }

    @Test
    fun `updateAddon - when extension is not installed`() {
        var updateStatus: Status? = null

        val manager = AddonManager(mock(), mock(), mock(), mock())

        manager.updateAddon("extensionId") { status ->
            updateStatus = status
        }

        assertEquals(Status.NotInstalled, updateStatus)
    }

    @Test
    fun `updateAddon - when extension is not supported`() {
        var updateStatus: Status? = null

        val extension: WebExtension = mock()
        whenever(extension.id).thenReturn("unsupportedExt")

        val metadata: Metadata = mock()
        whenever(metadata.disabledFlags).thenReturn(DisabledFlags.select(DisabledFlags.APP_SUPPORT))
        whenever(extension.getMetadata()).thenReturn(metadata)

        WebExtensionSupport.installedExtensions["extensionId"] = extension

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
        val addon = Addon(id = "ext1")
        val engine: Engine = mock()
        val onSuccessCaptor = argumentCaptor<((WebExtension) -> Unit)>()

        var installedAddon: Addon? = null
        val manager = AddonManager(mock(), engine, mock(), mock())
        manager.installAddon(
            addon,
            onSuccess = {
                installedAddon = it
            },
        )

        verify(engine).installWebExtension(
            eq("ext1"),
            any(),
            onSuccessCaptor.capture(),
            any(),
        )

        val extension: WebExtension = mock()
        whenever(extension.id).thenReturn("ext1")
        onSuccessCaptor.value.invoke(extension)
        assertNotNull(installedAddon)
        assertEquals(addon.id, installedAddon!!.id)
        assertTrue(manager.pendingAddonActions.isEmpty())
    }

    @Test
    fun `installAddon failure`() {
        val addon = Addon(id = "ext1")
        val engine: Engine = mock()
        val onErrorCaptor = argumentCaptor<((String, Throwable) -> Unit)>()

        var throwable: Throwable? = null
        var msg: String? = null
        val manager = AddonManager(mock(), engine, mock(), mock())
        manager.installAddon(
            addon,
            onError = { errorMsg, caught ->
                throwable = caught
                msg = errorMsg
            },
        )

        verify(engine).installWebExtension(
            eq("ext1"),
            any(),
            any(),
            onErrorCaptor.capture(),
        )

        onErrorCaptor.value.invoke(addon.id, IllegalStateException("test"))
        assertNotNull(throwable!!)
        assertEquals(msg, addon.id)
        assertTrue(manager.pendingAddonActions.isEmpty())
    }

    @Test
    fun `installAddon fails for blocked permission`() {
        val addon = Addon(
            id = "ext1",
            permissions = listOf("bookmarks", "geckoviewaddons", "nativemessaging"),
        )

        val engine: Engine = mock()

        var throwable: Throwable? = null
        var msg: String? = null
        val manager = AddonManager(mock(), engine, mock(), mock())
        manager.installAddon(
            addon,
            onError = { errorMsg, caught ->
                throwable = caught
                msg = errorMsg
            },
        )

        verify(engine, never()).installWebExtension(
            any(),
            any(),
            any(),
            any(),
        )

        assertNotNull(throwable!!)
        assertEquals(msg, addon.id)
        assertTrue(manager.pendingAddonActions.isEmpty())
    }

    @Test
    fun `uninstallAddon successfully`() {
        val installedAddon = Addon(
            id = "ext1",
            installedState = Addon.InstalledState("ext1", "1.0", "", true),
        )

        val extension: WebExtension = mock()
        whenever(extension.id).thenReturn("ext1")
        WebExtensionSupport.installedExtensions[installedAddon.id] = extension

        val engine: Engine = mock()
        val onSuccessCaptor = argumentCaptor<(() -> Unit)>()

        var successCallbackInvoked = false
        val manager = AddonManager(mock(), engine, mock(), mock())
        manager.uninstallAddon(
            installedAddon,
            onSuccess = {
                successCallbackInvoked = true
            },
        )
        verify(engine).uninstallWebExtension(eq(extension), onSuccessCaptor.capture(), any())

        onSuccessCaptor.value.invoke()
        assertTrue(successCallbackInvoked)
        assertTrue(manager.pendingAddonActions.isEmpty())
    }

    @Test
    fun `uninstallAddon failure cases`() {
        val addon = Addon(id = "ext1")
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
        assertTrue(manager.pendingAddonActions.isEmpty())
    }

    @Test
    fun `enableAddon successfully`() {
        val addon = Addon(
            id = "ext1",
            installedState = Addon.InstalledState("ext1", "1.0", "", true),
        )

        val extension: WebExtension = mock()
        whenever(extension.id).thenReturn("ext1")
        WebExtensionSupport.installedExtensions[addon.id] = extension

        val engine: Engine = mock()
        val onSuccessCaptor = argumentCaptor<((WebExtension) -> Unit)>()

        var enabledAddon: Addon? = null
        val manager = AddonManager(mock(), engine, mock(), mock())
        manager.enableAddon(
            addon,
            onSuccess = {
                enabledAddon = it
            },
        )

        verify(engine).enableWebExtension(eq(extension), any(), onSuccessCaptor.capture(), any())
        onSuccessCaptor.value.invoke(extension)
        assertNotNull(enabledAddon)
        assertEquals(addon.id, enabledAddon!!.id)
        assertTrue(manager.pendingAddonActions.isEmpty())
    }

    @Test
    fun `enableAddon failure cases`() {
        val addon = Addon(id = "ext1")
        val engine: Engine = mock()
        val onErrorCaptor = argumentCaptor<((Throwable) -> Unit)>()
        var throwable: Throwable? = null
        val errorCallback = { caught: Throwable ->
            throwable = caught
        }
        val manager = AddonManager(mock(), engine, mock(), mock())

        // Extension is not installed so we're invoking the error callback and never the engine
        manager.enableAddon(addon, onError = errorCallback)
        verify(engine, never()).enableWebExtension(any(), any(), any(), onErrorCaptor.capture())
        assertNotNull(throwable!!)
        assertEquals("Addon is not installed", throwable!!.localizedMessage)

        // Install extension and try again
        val extension: WebExtension = mock()
        whenever(extension.id).thenReturn("ext1")
        WebExtensionSupport.installedExtensions[addon.id] = extension
        manager.enableAddon(addon, onError = errorCallback)
        verify(engine, never()).enableWebExtension(any(), any(), any(), onErrorCaptor.capture())

        // Make sure engine error is forwarded to caller
        val installedAddon = addon.copy(installedState = Addon.InstalledState(addon.id, "1.0", "", true))
        manager.enableAddon(installedAddon, source = EnableSource.APP_SUPPORT, onError = errorCallback)
        verify(engine).enableWebExtension(eq(extension), eq(EnableSource.APP_SUPPORT), any(), onErrorCaptor.capture())
        onErrorCaptor.value.invoke(IllegalStateException("test"))
        assertNotNull(throwable!!)
        assertEquals("test", throwable!!.localizedMessage)
        assertTrue(manager.pendingAddonActions.isEmpty())
    }

    @Test
    fun `disableAddon successfully`() {
        val addon = Addon(
            id = "ext1",
            installedState = Addon.InstalledState("ext1", "1.0", "", true),
        )

        val extension: WebExtension = mock()
        whenever(extension.id).thenReturn("ext1")
        WebExtensionSupport.installedExtensions[addon.id] = extension

        val engine: Engine = mock()
        val onSuccessCaptor = argumentCaptor<((WebExtension) -> Unit)>()

        var disabledAddon: Addon? = null
        val manager = AddonManager(mock(), engine, mock(), mock())
        manager.disableAddon(
            addon,
            source = EnableSource.APP_SUPPORT,
            onSuccess = {
                disabledAddon = it
            },
        )

        verify(engine).disableWebExtension(eq(extension), eq(EnableSource.APP_SUPPORT), onSuccessCaptor.capture(), any())
        onSuccessCaptor.value.invoke(extension)
        assertNotNull(disabledAddon)
        assertEquals(addon.id, disabledAddon!!.id)
        assertTrue(manager.pendingAddonActions.isEmpty())
    }

    @Test
    fun `disableAddon failure cases`() {
        val addon = Addon(id = "ext1")
        val engine: Engine = mock()
        val onErrorCaptor = argumentCaptor<((Throwable) -> Unit)>()
        var throwable: Throwable? = null
        val errorCallback = { caught: Throwable ->
            throwable = caught
        }
        val manager = AddonManager(mock(), engine, mock(), mock())

        // Extension is not installed so we're invoking the error callback and never the engine
        manager.disableAddon(addon, onError = errorCallback)
        verify(engine, never()).disableWebExtension(any(), any(), any(), onErrorCaptor.capture())
        assertNotNull(throwable!!)
        assertEquals("Addon is not installed", throwable!!.localizedMessage)

        // Install extension and try again
        val extension: WebExtension = mock()
        whenever(extension.id).thenReturn("ext1")
        WebExtensionSupport.installedExtensions[addon.id] = extension
        manager.disableAddon(addon, onError = errorCallback)
        verify(engine, never()).disableWebExtension(any(), any(), any(), onErrorCaptor.capture())

        // Make sure engine error is forwarded to caller
        val installedAddon = addon.copy(installedState = Addon.InstalledState(addon.id, "1.0", "", true))
        manager.disableAddon(installedAddon, onError = errorCallback)
        verify(engine).disableWebExtension(eq(extension), any(), any(), onErrorCaptor.capture())
        onErrorCaptor.value.invoke(IllegalStateException("test"))
        assertNotNull(throwable!!)
        assertEquals("test", throwable!!.localizedMessage)
        assertTrue(manager.pendingAddonActions.isEmpty())
    }

    @Test
    fun `getDisabledReason cases`() {
        val extension: WebExtension = mock()
        val metadata: Metadata = mock()

        whenever(metadata.disabledFlags).thenReturn(DisabledFlags.select(BLOCKLIST))
        whenever(extension.getMetadata()).thenReturn(metadata)

        assertEquals(Addon.DisabledReason.BLOCKLISTED, extension.getDisabledReason())

        whenever(metadata.disabledFlags).thenReturn(DisabledFlags.select(APP_SUPPORT))

        assertEquals(Addon.DisabledReason.UNSUPPORTED, extension.getDisabledReason())

        whenever(metadata.disabledFlags).thenReturn(DisabledFlags.select(USER))

        assertEquals(Addon.DisabledReason.USER_REQUESTED, extension.getDisabledReason())

        whenever(metadata.disabledFlags).thenReturn(DisabledFlags.select(0))

        assertNull(extension.getDisabledReason())
    }
}
