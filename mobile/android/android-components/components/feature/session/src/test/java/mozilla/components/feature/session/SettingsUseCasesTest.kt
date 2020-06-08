/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy
import mozilla.components.concept.engine.Settings
import mozilla.components.feature.session.SettingsUseCases.UpdateSettingUseCase
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.verify

class SettingsUseCasesTest {

    private lateinit var settings: Settings
    private lateinit var sessionManager: SessionManager
    private lateinit var sessionA: Session
    private lateinit var engineSessionA: EngineSession
    private lateinit var settingsA: Settings
    private lateinit var sessionB: Session
    private lateinit var engineSessionB: EngineSession
    private lateinit var settingsB: Settings
    private lateinit var sessionC: Session
    private lateinit var engine: Engine
    private lateinit var useCases: SettingsUseCases

    @Before
    fun setup() {
        settings = mock()
        sessionManager = mock()
        sessionA = mock()
        engineSessionA = mock()
        settingsA = mock()
        sessionB = mock()
        engineSessionB = mock()
        settingsB = mock()
        sessionC = mock()
        engine = mock()
        whenever(engine.settings).thenReturn(settings)
        useCases = SettingsUseCases(engine, sessionManager)

        whenever(sessionManager.sessions).thenReturn(listOf(sessionA, sessionB, sessionC))
        whenever(sessionManager.getEngineSession(sessionA)).thenReturn(engineSessionA)
        whenever(sessionManager.getEngineSession(sessionB)).thenReturn(engineSessionB)
        whenever(sessionManager.getEngineSession(sessionC)).thenReturn(null)
        whenever(engineSessionA.settings).thenReturn(settingsA)
        whenever(engineSessionB.settings).thenReturn(settingsB)
    }

    @Test
    fun `UpdateSettingUseCase will update all sessions`() {
        val allowFileAccessSetting = object : UpdateSettingUseCase<Boolean>(engine, sessionManager) {
            override fun update(settings: Settings, value: Boolean) {
                settings.allowFileAccess = value
            }
        }

        allowFileAccessSetting(true)
        verify(settings).allowFileAccess = true
        verify(engineSessionA.settings).allowFileAccess = true
        verify(engineSessionB.settings).allowFileAccess = true
    }

    @Test
    fun `UpdateSettingUseCase will clear speculative engine session`() {
        val allowFileAccessSetting = object : UpdateSettingUseCase<Boolean>(engine, sessionManager) {
            override fun update(settings: Settings, value: Boolean) {
                settings.allowFileAccess = value
            }
        }

        allowFileAccessSetting(true)
        verify(engine).clearSpeculativeSession()
    }

    @Test
    fun updateTrackingProtection() {
        useCases.updateTrackingProtection(TrackingProtectionPolicy.none())
        verify(settings).trackingProtectionPolicy = TrackingProtectionPolicy.none()
        verify(engineSessionA).enableTrackingProtection(TrackingProtectionPolicy.none())
        verify(engineSessionB).enableTrackingProtection(TrackingProtectionPolicy.none())

        useCases.updateTrackingProtection(TrackingProtectionPolicy.strict())
        verify(settings).trackingProtectionPolicy = TrackingProtectionPolicy.strict()
        verify(engineSessionA).enableTrackingProtection(TrackingProtectionPolicy.strict())
        verify(engineSessionB).enableTrackingProtection(TrackingProtectionPolicy.strict())
    }
}
