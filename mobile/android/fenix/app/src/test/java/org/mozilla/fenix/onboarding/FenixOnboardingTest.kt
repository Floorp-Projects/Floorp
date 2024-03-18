/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding

import android.content.Context
import android.content.Context.MODE_PRIVATE
import android.content.SharedPreferences
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.helpers.perf.TestStrictModeManager
import org.mozilla.fenix.onboarding.FenixOnboarding.Companion.CURRENT_ONBOARDING_VERSION
import org.mozilla.fenix.onboarding.FenixOnboarding.Companion.LAST_VERSION_ONBOARDING_KEY
import org.mozilla.fenix.perf.StrictModeManager
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class FenixOnboardingTest {

    private lateinit var onboarding: FenixOnboarding
    private lateinit var preferences: SharedPreferences
    private lateinit var preferencesEditor: SharedPreferences.Editor
    private lateinit var settings: Settings

    @Before
    fun setup() {
        preferences = mockk()
        preferencesEditor = mockk(relaxed = true)
        settings = mockk(relaxed = true)
        val context = mockk<Context>()
        every { preferences.edit() } returns preferencesEditor
        every { context.components.strictMode } returns TestStrictModeManager() as StrictModeManager
        every { context.getSharedPreferences(any(), MODE_PRIVATE) } returns preferences
        every { context.settings() } returns settings

        onboarding = FenixOnboarding(context)
    }

    @Test
    fun testUserHasBeenOnboarded() {
        every {
            preferences.getInt(LAST_VERSION_ONBOARDING_KEY, any())
        } returns 0
        assertFalse(onboarding.userHasBeenOnboarded())

        every {
            preferences.getInt(LAST_VERSION_ONBOARDING_KEY, any())
        } returns CURRENT_ONBOARDING_VERSION
        assertTrue(onboarding.userHasBeenOnboarded())
    }

    @Test
    fun testFinish() {
        settings.showHomeOnboardingDialog = true

        onboarding.finish()

        assertFalse(settings.showHomeOnboardingDialog)
        verify { preferencesEditor.putInt(LAST_VERSION_ONBOARDING_KEY, CURRENT_ONBOARDING_VERSION) }
    }
}
