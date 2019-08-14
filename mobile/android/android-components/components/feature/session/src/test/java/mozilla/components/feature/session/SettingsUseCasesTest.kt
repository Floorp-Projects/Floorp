package mozilla.components.feature.session

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
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

    private val settings = mock<Settings>()
    private val sessionManager = mock<SessionManager>()
    private val sessionA = mock<Session>()
    private val engineSessionA = mock<EngineSession>()
    private val settingsA = mock<Settings>()
    private val sessionB = mock<Session>()
    private val engineSessionB = mock<EngineSession>()
    private val settingsB = mock<Settings>()
    private val sessionC = mock<Session>()
    private val useCases = SettingsUseCases(settings, sessionManager)

    @Before
    fun setup() {
        whenever(sessionManager.sessions).thenReturn(listOf(sessionA, sessionB, sessionC))
        whenever(sessionManager.getEngineSession(sessionA)).thenReturn(engineSessionA)
        whenever(sessionManager.getEngineSession(sessionB)).thenReturn(engineSessionB)
        whenever(sessionManager.getEngineSession(sessionC)).thenReturn(null)
        whenever(engineSessionA.settings).thenReturn(settingsA)
        whenever(engineSessionB.settings).thenReturn(settingsB)
    }

    @Test
    fun `UpdateSettingUseCase will update all sessions`() {
        val autoplaySetting = object : UpdateSettingUseCase<Boolean>(settings, sessionManager) {
            override fun update(settings: Settings, value: Boolean) {
                settings.allowAutoplayMedia = value
            }
        }

        autoplaySetting(true)
        verify(settings).allowAutoplayMedia = true
        verify(engineSessionA.settings).allowAutoplayMedia = true
        verify(engineSessionB.settings).allowAutoplayMedia = true
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
