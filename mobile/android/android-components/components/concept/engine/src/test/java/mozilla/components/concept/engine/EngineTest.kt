/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine

import android.content.Context
import android.util.AttributeSet
import android.util.JsonReader
import mozilla.components.concept.base.profiler.Profiler
import mozilla.components.concept.engine.Engine.BrowsingData
import mozilla.components.concept.engine.utils.EngineVersion
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.lang.UnsupportedOperationException

class EngineTest {

    private val testEngine = object : Engine {
        override val version: EngineVersion
            get() = throw NotImplementedError("Not needed for test")

        override fun createView(context: Context, attrs: AttributeSet?): EngineView {
            throw NotImplementedError("Not needed for test")
        }

        override fun createSession(private: Boolean, contextId: String?): EngineSession {
            throw NotImplementedError("Not needed for test")
        }

        override fun createSessionState(json: JSONObject): EngineSessionState {
            throw NotImplementedError("Not needed for test")
        }

        override fun createSessionStateFrom(reader: JsonReader): EngineSessionState {
            throw NotImplementedError("Not needed for test")
        }

        override fun name(): String {
            throw NotImplementedError("Not needed for test")
        }

        override fun speculativeConnect(url: String) {
            throw NotImplementedError("Not needed for test")
        }

        override val profiler: Profiler?
            get() = throw NotImplementedError("Not needed for test")

        override val settings: Settings
            get() = throw NotImplementedError("Not needed for test")
    }

    @Test
    fun `invokes default functions on trackingProtectionExceptionStore`() {
        var wasExecuted = false
        try {
            testEngine.trackingProtectionExceptionStore
        } catch (_: Exception) {
            wasExecuted = true
        }
        assertTrue(wasExecuted)
    }

    @Test
    fun `invokes error callback if webextensions not supported`() {
        var exception: Throwable? = null
        testEngine.installWebExtension("my-ext", "resource://path", onError = { _, e -> exception = e })
        assertNotNull(exception)
        assertTrue(exception is UnsupportedOperationException)

        exception = null
        testEngine.listInstalledWebExtensions(onSuccess = { }, onError = { e -> exception = e })
        assertNotNull(exception)
        assertTrue(exception is UnsupportedOperationException)
    }

    @Test
    fun `invokes error callback if clear data not supported`() {
        var exception: Throwable? = null
        testEngine.clearData(Engine.BrowsingData.all(), onError = { exception = it })
        assertNotNull(exception)
        assertTrue(exception is UnsupportedOperationException)
    }

    @Test
    fun `browsing data types can be combined`() {
        assertEquals(BrowsingData.ALL, BrowsingData.all().types)
        assertTrue(BrowsingData.all().contains(BrowsingData.NETWORK_CACHE))
        assertTrue(BrowsingData.all().contains(BrowsingData.IMAGE_CACHE))
        assertTrue(BrowsingData.all().contains(BrowsingData.PERMISSIONS))
        assertTrue(BrowsingData.all().contains(BrowsingData.DOM_STORAGES))
        assertTrue(BrowsingData.all().contains(BrowsingData.COOKIES))
        assertTrue(BrowsingData.all().contains(BrowsingData.AUTH_SESSIONS))
        assertTrue(BrowsingData.all().contains(BrowsingData.allSiteSettings().types))
        assertTrue(BrowsingData.all().contains(BrowsingData.allSiteData().types))
        assertTrue(BrowsingData.all().contains(BrowsingData.allCaches().types))

        assertEquals(BrowsingData.ALL_CACHES, BrowsingData.allCaches().types)
        assertTrue(BrowsingData.allCaches().contains(BrowsingData.NETWORK_CACHE))
        assertTrue(BrowsingData.allCaches().contains(BrowsingData.IMAGE_CACHE))
        assertFalse(BrowsingData.allCaches().contains(BrowsingData.PERMISSIONS))
        assertFalse(BrowsingData.allCaches().contains(BrowsingData.AUTH_SESSIONS))
        assertFalse(BrowsingData.allCaches().contains(BrowsingData.COOKIES))
        assertFalse(BrowsingData.allCaches().contains(BrowsingData.DOM_STORAGES))

        assertEquals(BrowsingData.ALL_SITE_DATA, BrowsingData.allSiteData().types)
        assertTrue(BrowsingData.allSiteData().contains(BrowsingData.NETWORK_CACHE))
        assertTrue(BrowsingData.allSiteData().contains(BrowsingData.IMAGE_CACHE))
        assertTrue(BrowsingData.allSiteData().contains(BrowsingData.PERMISSIONS))
        assertTrue(BrowsingData.allSiteData().contains(BrowsingData.DOM_STORAGES))
        assertTrue(BrowsingData.allSiteData().contains(BrowsingData.COOKIES))
        assertFalse(BrowsingData.allSiteData().contains(BrowsingData.AUTH_SESSIONS))

        assertEquals(BrowsingData.ALL_SITE_SETTINGS, BrowsingData.allSiteSettings().types)
        assertTrue(BrowsingData.allSiteSettings().contains(BrowsingData.PERMISSIONS))
        assertFalse(BrowsingData.allSiteSettings().contains(BrowsingData.IMAGE_CACHE))
        assertFalse(BrowsingData.allSiteSettings().contains(BrowsingData.NETWORK_CACHE))
        assertFalse(BrowsingData.allSiteSettings().contains(BrowsingData.AUTH_SESSIONS))
        assertFalse(BrowsingData.allSiteSettings().contains(BrowsingData.COOKIES))
        assertFalse(BrowsingData.allSiteSettings().contains(BrowsingData.DOM_STORAGES))

        val browsingData = BrowsingData.select(BrowsingData.COOKIES, BrowsingData.DOM_STORAGES)
        assertTrue(browsingData.contains(BrowsingData.COOKIES))
        assertTrue(browsingData.contains(BrowsingData.DOM_STORAGES))
        assertFalse(browsingData.contains(BrowsingData.AUTH_SESSIONS))
        assertFalse(browsingData.contains(BrowsingData.IMAGE_CACHE))
        assertFalse(browsingData.contains(BrowsingData.NETWORK_CACHE))
        assertFalse(browsingData.contains(BrowsingData.PERMISSIONS))
    }
}
