/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy
import mozilla.components.concept.engine.Settings
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import org.junit.Test
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.reset
import org.mockito.Mockito.verify

class SettingsUseCasesTest {
    @Test
    fun updateTrackingProtection() {
        val engineSessionA: EngineSession = mock()
        val engineSessionB: EngineSession = mock()

        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "A"),
                    createTab("https://www.mozilla.org", id = "B"),
                ),
                selectedTabId = "A",
            ),
        )

        store.dispatch(
            EngineAction.LinkEngineSessionAction(
                tabId = "A",
                engineSession = engineSessionA,
            ),
        ).joinBlocking()

        store.dispatch(
            EngineAction.LinkEngineSessionAction(
                tabId = "B",
                engineSession = engineSessionB,
            ),
        ).joinBlocking()

        val engine: Engine = mock()
        val settings: Settings = mock()
        doReturn(settings).`when`(engine).settings

        val useCases = SettingsUseCases(engine, store)

        useCases.updateTrackingProtection(TrackingProtectionPolicy.none())
        verify(settings).trackingProtectionPolicy = TrackingProtectionPolicy.none()
        verify(engineSessionA).updateTrackingProtection(TrackingProtectionPolicy.none())
        verify(engineSessionB).updateTrackingProtection(TrackingProtectionPolicy.none())
        verify(engine).clearSpeculativeSession()

        reset(engine)
        doReturn(settings).`when`(engine).settings

        useCases.updateTrackingProtection(TrackingProtectionPolicy.strict())
        verify(settings).trackingProtectionPolicy = TrackingProtectionPolicy.strict()
        verify(engineSessionA).updateTrackingProtection(TrackingProtectionPolicy.strict())
        verify(engineSessionB).updateTrackingProtection(TrackingProtectionPolicy.strict())
        verify(engine).clearSpeculativeSession()
    }
}
