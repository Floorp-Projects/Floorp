package org.mozilla.fenix

import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.just
import io.mockk.runs
import io.mockk.verify
import kotlinx.coroutines.ObsoleteCoroutinesApi
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.utils.Settings
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@ObsoleteCoroutinesApi
@RunWith(RobolectricTestRunner::class)
@Config(application = TestApplication::class)
class FenixApplicationTest {

    lateinit var application: FenixApplication
    @MockK lateinit var settings: Settings

    @Before
    fun before() {
        MockKAnnotations.init(this)
        every { settings setProperty "usePrivateMode" value any<Boolean>() } just runs
        application = TestApplication()
    }

    @Test
    fun `GIVEN openLinksInAPrivateTab is active WHEN maybeClearPrivateMode is called THEN private mode should not be changed`() {
        every { settings.openLinksInAPrivateTab } returns true

        application.maybeClearPrivateMode(settings)

        verify(exactly = 0) { settings.usePrivateMode = any<Boolean>() }
    }

    @Test
    fun `GIVEN openLinksInAPrivateTab is inactive WHEN maybeClearPrivateMode is called THEN private mode should be disabled`() {
        every { settings.openLinksInAPrivateTab } returns false

        application.maybeClearPrivateMode(settings)

        verify(exactly = 1) { settings.usePrivateMode = false }
    }
}
