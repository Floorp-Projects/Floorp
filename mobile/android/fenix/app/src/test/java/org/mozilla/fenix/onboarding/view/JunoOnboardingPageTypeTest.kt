package org.mozilla.fenix.onboarding.view

import org.junit.Assert.assertEquals
import org.junit.Test

class JunoOnboardingPageTypeTest {

    @Test
    fun `GIVEN a list of JunoOnboardingPageType sequenceId() should map to the correct sequence id - 1`() {
        val list = listOf(
            JunoOnboardingPageType.DEFAULT_BROWSER,
            JunoOnboardingPageType.SYNC_SIGN_IN,
            JunoOnboardingPageType.NOTIFICATION_PERMISSION,
        )

        val expected = "default_sync_notification"
        val actual = list.telemetrySequenceId()

        assertEquals(expected, actual)
    }

    @Test
    fun `GIVEN a list of JunoOnboardingPageType sequenceId() should map to the correct sequence id - 2`() {
        val list = listOf(
            JunoOnboardingPageType.DEFAULT_BROWSER,
            JunoOnboardingPageType.SYNC_SIGN_IN,
        )

        val expected = "default_sync"
        val actual = list.telemetrySequenceId()

        assertEquals(expected, actual)
    }

    @Test
    fun `GIVEN a list of JunoOnboardingPageType sequenceId() should map to the correct sequence id - 3`() {
        val list = listOf(JunoOnboardingPageType.DEFAULT_BROWSER)

        val expected = "default"
        val actual = list.telemetrySequenceId()

        assertEquals(expected, actual)
    }
}
