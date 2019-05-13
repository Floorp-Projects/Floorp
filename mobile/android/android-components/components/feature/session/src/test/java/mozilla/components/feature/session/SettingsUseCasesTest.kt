package mozilla.components.feature.session

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy
import mozilla.components.concept.engine.Settings
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.*

class SettingsUseCasesTest {
    private val settings = mock(Settings::class.java)
    private val sessionManager = mock(SessionManager::class.java)
    private val engineSessionA = mock(EngineSession::class.java)
    private val sessionA = mock(Session::class.java)
    private val engineSessionB = mock(EngineSession::class.java)
    private val sessionB = mock(Session::class.java)
    private val useCases = SettingsUseCases(settings, sessionManager)

    @Before
    fun setup() {
        `when`(sessionManager.sessions).thenReturn(listOf(sessionA, sessionB))
        `when`(sessionManager.getEngineSession(sessionA)).thenReturn(engineSessionA)
        `when`(sessionManager.getEngineSession(sessionB)).thenReturn(engineSessionB)
    }

    @Test
    fun updateTrackingProtection() {
        useCases.updateTrackingProtection.invoke(TrackingProtectionPolicy.none())
        verify(settings).trackingProtectionPolicy = TrackingProtectionPolicy.none()
        verify(engineSessionA).enableTrackingProtection(TrackingProtectionPolicy.none())
        verify(engineSessionB).enableTrackingProtection(TrackingProtectionPolicy.none())

        useCases.updateTrackingProtection.invoke(TrackingProtectionPolicy.all())
        verify(settings).trackingProtectionPolicy = TrackingProtectionPolicy.all()
        verify(engineSessionA).enableTrackingProtection(TrackingProtectionPolicy.all())
        verify(engineSessionB).enableTrackingProtection(TrackingProtectionPolicy.all())
    }
}
