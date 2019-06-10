/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa

import android.content.Context
import android.content.pm.PackageManager
import android.content.pm.ShortcutInfo
import android.content.pm.ShortcutManager
import android.os.Build
import androidx.core.content.pm.ShortcutInfoCompat
import androidx.core.net.toUri
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.Mock
import org.mockito.Mockito.`when`
import org.mockito.Mockito.clearInvocations
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.MockitoAnnotations.initMocks
import org.robolectric.util.ReflectionHelpers.setStaticField
import kotlin.reflect.jvm.javaField

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class WebAppShortcutManagerTest {
    @Mock
    lateinit var context: Context
    @Mock
    lateinit var packageManager: PackageManager
    @Mock
    lateinit var shortcutManager: ShortcutManager
    @Mock
    internal lateinit var storage: ManifestStorage

    @Before
    fun setup() {
        setSdkInt(0)
        initMocks(this)

        `when`(context.packageManager).thenReturn(packageManager)
        `when`(context.getSystemService(ShortcutManager::class.java)).thenReturn(shortcutManager)
    }

    @After
    fun teardown() = setSdkInt(0)

    @Test
    fun `requestPinShortcut no-op if pinning unsupported`() = runBlockingTest {
        val manager = spy(WebAppShortcutManager(storage))
        val manifest = WebAppManifest(name = "Demo", startUrl = "https://example.com")
        `when`(packageManager.queryBroadcastReceivers(any(), anyInt())).thenReturn(emptyList())

        manager.requestPinShortcut(context, manifest)
        verify(manager, never()).buildShortcut(context, manifest)

        setSdkInt(Build.VERSION_CODES.O)
        `when`(shortcutManager.isRequestPinShortcutSupported).thenReturn(false)
        clearInvocations(manager)

        manager.requestPinShortcut(context, manifest)
        verify(manager, never()).buildShortcut(context, manifest)
    }

    @Test
    fun `requestPinShortcut won't pin if null shortcut is built`() = runBlockingTest {
        setSdkInt(Build.VERSION_CODES.O)
        val manager = spy(WebAppShortcutManager(storage))
        val manifest = WebAppManifest(name = "Demo", startUrl = "https://example.com")
        `when`(shortcutManager.isRequestPinShortcutSupported).thenReturn(true)
        doReturn(null).`when`(manager).buildShortcut(context, manifest)

        manager.requestPinShortcut(context, manifest)
        verify(manager).buildShortcut(context, manifest)
        verify(shortcutManager, never()).requestPinShortcut(any(), any())
    }

    @Test
    fun `requestPinShortcut pins shortcut`() = runBlockingTest {
        setSdkInt(Build.VERSION_CODES.O)
        val manager = spy(WebAppShortcutManager(storage))
        val manifest = WebAppManifest(name = "Demo", startUrl = "https://example.com")
        val shortcutCompat: ShortcutInfoCompat = mock()
        `when`(shortcutManager.isRequestPinShortcutSupported).thenReturn(true)
        doReturn(shortcutCompat).`when`(manager).buildShortcut(context, manifest)

        manager.requestPinShortcut(context, manifest)
        verify(manager).buildShortcut(context, manifest)
        verify(shortcutManager).requestPinShortcut(any(), any())
    }

    @Test
    fun `updateShortcuts no-op`() = runBlockingTest {
        val manager = spy(WebAppShortcutManager(storage))
        val manifests = listOf(WebAppManifest(name = "Demo", startUrl = "https://example.com"))
        doReturn(null).`when`(manager).buildShortcut(context, manifests[0])

        manager.updateShortcuts(context, manifests)
        verify(manager, never()).buildShortcut(context, manifests[0])
        verify(shortcutManager, never()).updateShortcuts(any())

        setSdkInt(Build.VERSION_CODES.N_MR1)
        manager.updateShortcuts(context, manifests)
        verify(shortcutManager).updateShortcuts(emptyList())
    }

    @Test
    fun `updateShortcuts updates list of existing shortcuts`() = runBlockingTest {
        setSdkInt(Build.VERSION_CODES.N_MR1)
        val manager = spy(WebAppShortcutManager(storage))
        val manifests = listOf(WebAppManifest(name = "Demo", startUrl = "https://example.com"))
        val shortcutCompat: ShortcutInfoCompat = mock()
        val shortcut: ShortcutInfo = mock()
        doReturn(shortcutCompat).`when`(manager).buildShortcut(context, manifests[0])
        doReturn(shortcut).`when`(shortcutCompat).toShortcutInfo()

        manager.updateShortcuts(context, manifests)
        verify(shortcutManager).updateShortcuts(listOf(shortcut))
    }

    @Test
    fun `buildShortcut builds shortcut and saves manifest`() = runBlockingTest {
        val manager = WebAppShortcutManager(storage)
        val manifest = WebAppManifest(name = "Demo", startUrl = "https://example.com")
        val shortcut = manager.buildShortcut(context, manifest)!!
        val intent = shortcut.intent

        verify(storage).saveManifest(manifest)

        assertEquals("https://example.com", shortcut.id)
        assertEquals("Demo", shortcut.longLabel)
        assertEquals("Demo", shortcut.shortLabel)
        assertEquals(WebAppLauncherActivity.INTENT_ACTION, intent.action)
        assertEquals("https://example.com".toUri(), intent.data)
    }

    @Test
    fun `buildShortcut builds shortcut with short name`() = runBlockingTest {
        val manager = WebAppShortcutManager(storage)
        val manifest = WebAppManifest(name = "Demo Demo", shortName = "DD", startUrl = "https://example.com")
        val shortcut = manager.buildShortcut(context, manifest)!!

        assertEquals("https://example.com", shortcut.id)
        assertEquals("Demo Demo", shortcut.longLabel)
        assertEquals("DD", shortcut.shortLabel)
    }

    @Test
    fun `findShortcut returns shortcut`() {
        val manager = WebAppShortcutManager(storage)
        assertNull(manager.findShortcut(context, "https://mozilla.org"))

        setSdkInt(Build.VERSION_CODES.N_MR1)
        val exampleShortcut = mock<ShortcutInfo>().apply {
            `when`(id).thenReturn("https://example.com")
        }
        `when`(shortcutManager.pinnedShortcuts).thenReturn(listOf(exampleShortcut))

        assertNull(manager.findShortcut(context, "https://mozilla.org"))

        val mozShortcut = mock<ShortcutInfo>().apply {
            `when`(id).thenReturn("https://mozilla.org")
        }
        `when`(shortcutManager.pinnedShortcuts).thenReturn(listOf(mozShortcut, exampleShortcut))

        assertEquals(mozShortcut, manager.findShortcut(context, "https://mozilla.org"))
    }

    @Test
    fun `uninstallShortcuts removes shortcut`() = runBlockingTest {
        val manager = WebAppShortcutManager(storage)
        manager.uninstallShortcuts(context, listOf("https://mozilla.org"))
        verify(shortcutManager, never()).disableShortcuts(listOf("https://mozilla.org"), null)
        verify(storage).removeManifests(listOf("https://mozilla.org"))

        clearInvocations(shortcutManager)
        clearInvocations(storage)

        setSdkInt(Build.VERSION_CODES.N_MR1)
        manager.uninstallShortcuts(context, listOf("https://mozilla.org"))
        verify(shortcutManager).disableShortcuts(listOf("https://mozilla.org"), null)
        verify(storage).removeManifests(listOf("https://mozilla.org"))
    }

    @Test
    fun `uninstallShortcuts sets disabledMessage`() = runBlockingTest {
        setSdkInt(Build.VERSION_CODES.N_MR1)
        val domains = listOf("https://mozilla.org", "https://firefox.com")
        val message = "Can't touch this - its uninstalled."
        WebAppShortcutManager(storage).uninstallShortcuts(context, domains, message)

        verify(shortcutManager).disableShortcuts(domains, message)
    }

    private fun setSdkInt(sdkVersion: Int) {
        setStaticField(Build.VERSION::SDK_INT.javaField, sdkVersion)
    }
}
