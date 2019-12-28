package org.mozilla.fenix.session

import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.support.test.robolectric.testContext
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.TestApplication
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import mozilla.components.browser.session.Session

@RunWith(RobolectricTestRunner::class)
@Config(application = TestApplication::class)
class NotificationSessionObserverTest {

    private lateinit var observer: NotificationSessionObserver
    @MockK(relaxed = true) private lateinit var notificationService: SessionNotificationService.Companion

    @Before
    fun before() {
        MockKAnnotations.init(this)
        observer = NotificationSessionObserver(testContext, notificationService)
    }

    @Test
    fun `GIVEN session is private and non-custom WHEN it is added THEN notification service should be started`() {
        val privateSession = mockSession(true, false)

        observer.onSessionAdded(privateSession)
        verify(exactly = 1) { notificationService.start(any()) }
    }

    @Test
    fun `GIVEN session is not private WHEN it is added THEN notification service should not be started`() {
        val normalSession = mockSession(false, true)
        val customSession = mockSession(false, false)

        observer.onSessionAdded(normalSession)
        verify(exactly = 0) { notificationService.start(any()) }

        observer.onSessionAdded(customSession)
        verify(exactly = 0) { notificationService.start(any()) }
    }

    @Test
    fun `GIVEN session is custom tab WHEN it is added THEN notification service should not be started`() {
        val privateCustomSession = mockSession(true, true)
        val customSession = mockSession(false, true)

        observer.onSessionAdded(privateCustomSession)
        verify(exactly = 0) { notificationService.start(any()) }

        observer.onSessionAdded(customSession)
        verify(exactly = 0) { notificationService.start(any()) }
    }
}

private fun mockSession(isPrivate: Boolean, isCustom: Boolean) = mockk<Session> {
    every { private } returns isPrivate
    every { isCustomTabSession() } returns isCustom
}
