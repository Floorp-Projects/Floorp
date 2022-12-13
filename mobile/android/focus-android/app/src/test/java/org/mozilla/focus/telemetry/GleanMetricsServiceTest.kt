/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.telemetry

import android.content.Context
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.search.telemetry.ads.AdsTelemetry
import mozilla.components.feature.search.telemetry.incontent.InContentTelemetry
import org.junit.Test
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.mock
import org.mockito.Mockito.verify
import org.mozilla.focus.Components

class GleanMetricsServiceTest {
    @Test
    fun `WHEN installSearchTelemetryExtensions is called THEN install the ads and search telemetry extensions`() {
        val components = mock(Components::class.java)
        val store = mock(BrowserStore::class.java)
        val engine = mock(Engine::class.java)
        val adsExtension = mock(AdsTelemetry::class.java)
        val searchExtension = mock(InContentTelemetry::class.java)
        doReturn(engine).`when`(components).engine
        doReturn(store).`when`(components).store
        doReturn(adsExtension).`when`(components).adsTelemetry
        doReturn(searchExtension).`when`(components).searchTelemetry
        val glean = GleanMetricsService(mock(Context::class.java))

        glean.installSearchTelemetryExtensions(components)

        verify(adsExtension).install(engine, store)
        verify(searchExtension).install(engine, store)
    }
}
