package mozilla.components.feature.p2p

import android.content.pm.PackageManager
import android.view.View
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.feature.p2p.view.P2PBar
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.lib.nearby.NearbyConnection
import mozilla.components.support.base.feature.OnNeedToRequestPermissions
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import mozilla.components.support.webextensions.WebExtensionController
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito

@RunWith(AndroidJUnit4::class)
class P2PFeatureTest {
    private val mockP2PView = mock<P2PBar>()
    private val mockView = mock<View>()
    private var requestedPermissions: List<String>? = null // mutated by onNeedToRequestPermissions
    private val onNeedToRequestPerms =
        { perms: Array<String> -> requestedPermissions = perms.toList() }
    private val neededPermissions = NearbyConnection.PERMISSIONS + P2PFeature.LOCAL_PERMISSIONS

    @Before
    fun setup() {
        WebExtensionController.installedExtensions.clear()
        requestedPermissions = null
        whenever(mockP2PView.asView()).thenReturn(mockView)
        whenever(mockView.context).thenReturn(InstrumentationRegistry.getInstrumentation().context)
    }

    private fun createP2PFeature(
        browserStore: BrowserStore = BrowserStore(),
        engine: Engine = mock(),
        thunk: (() -> NearbyConnection) = { mock() },
        tabsUseCases: TabsUseCases = mock(),
        sessionUseCases: SessionUseCases = mock(),
        onNeedToRequestPermissions: OnNeedToRequestPermissions = onNeedToRequestPerms,
        onClose: (() -> Unit) = { }
    ): P2PFeature =
        P2PFeature(
            mockP2PView,
            browserStore,
            engine,
            thunk,
            tabsUseCases,
            sessionUseCases,
            onNeedToRequestPermissions,
            onClose
        )

    @Test
    fun `start does not proceed if permissions ungranted`() {
        val p2pFeature = createP2PFeature()
        p2pFeature.start()

        // Because no permissions are granted by the instrumentation context,
        // all will be requested. Make sure this contains all of the needed permissions.
        assertNotNull("No permissions were requested.", requestedPermissions)
        requestedPermissions?.run {
            neededPermissions.forEach {
                assertTrue(this.contains(it))
            }
        }

        // Deny one permission, which will prevent the P2PController from being constructed
        // and keep the web extension from being installed.
        p2pFeature.onPermissionsResult(
            neededPermissions,
            intArrayOf(PackageManager.PERMISSION_DENIED) +
                IntArray(neededPermissions.size - 1) { PackageManager.PERMISSION_GRANTED }
        )
        assertNull(p2pFeature.interactor)
        assertNull(p2pFeature.presenter)
    }

    @Test
    fun `start registers observer for selected session`() {
        val p2pFeature = Mockito.spy(createP2PFeature())
        p2pFeature.start()

        // Grant all permissions.
        p2pFeature.onPermissionsResult(
            neededPermissions,
            IntArray(neededPermissions.size) { PackageManager.PERMISSION_GRANTED }
        )

        assertNotNull(p2pFeature.interactor)
        assertNotNull(p2pFeature.presenter)
        assertNotNull(p2pFeature.extensionController)
    }

    @Test
    fun `start creates controller`() {
        val p2pFeature = createP2PFeature()
        p2pFeature.start()

        // Grant all permissions.
        p2pFeature.onPermissionsResult(
            neededPermissions,
            IntArray(neededPermissions.size) { PackageManager.PERMISSION_GRANTED }
        )

        assertNotNull(p2pFeature.interactor)
        assertNotNull(p2pFeature.presenter)
    }

    @Test
    fun `start installs webextension`() {
        val engine = mock<Engine>()
        val p2pFeature = createP2PFeature(engine = engine)
        p2pFeature.start()

        // Grant all permissions.
        p2pFeature.onPermissionsResult(
            neededPermissions,
            IntArray(neededPermissions.size) { PackageManager.PERMISSION_GRANTED }
        )

        val onSuccess = argumentCaptor<((WebExtension) -> Unit)>()
        val onError = argumentCaptor<((String, Throwable) -> Unit)>()
        Mockito.verify(engine, Mockito.times(1)).installWebExtension(
            eq(P2PFeature.P2P_EXTENSION_ID),
            eq(P2PFeature.P2P_EXTENSION_URL),
            onSuccess.capture(),
            onError.capture()
        )

        onSuccess.value.invoke(mock())

        // Already installed, should not try to install again.
        Mockito.verify(engine, Mockito.times(1)).installWebExtension(
            eq(P2PFeature.P2P_EXTENSION_ID),
            eq(P2PFeature.P2P_EXTENSION_URL),
            onSuccess.capture(),
            onError.capture()
        )
    }
}