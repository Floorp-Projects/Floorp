/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.serviceworker

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.engine.gecko.GeckoEngine
import mozilla.components.browser.state.state.SessionState
import mozilla.components.concept.engine.DefaultSettings
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags
import mozilla.components.feature.tabs.TabsUseCases.AddNewTabUseCase
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertNotNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify
import org.mozilla.geckoview.GeckoRuntime

@RunWith(AndroidJUnit4::class)
class ServiceWorkerSupportTest {
    @Test
    fun `GIVEN service worker support is installed WHEN runtime is called to open a new window THEN do so and return a new EngineSession`() {
        val runtime = GeckoRuntime.getDefault(testContext)
        val settings = DefaultSettings()
        val engine = GeckoEngine(testContext, runtime = runtime, defaultSettings = settings)
        val addNewTabUseCase = mock<AddNewTabUseCase>()
        ServiceWorkerSupport.install(engine, addNewTabUseCase)

        val result = runtime.serviceWorkerDelegate!!.onOpenWindow("")

        assertNotNull(result.poll(1))
        verify(addNewTabUseCase).invoke(
            url = eq("about:blank"),
            selectTab = eq(true), // default
            startLoading = eq(true), // default
            parentId = eq(null), // default
            flags = eq(LoadUrlFlags.external()),
            contextId = eq(null), // default
            engineSession = any<EngineSession>(),
            source = eq(SessionState.Source.Internal.None),
            searchTerms = eq(""), // default
            private = eq(false), // default
            historyMetadata = eq(null), // default
        )
    }
}
