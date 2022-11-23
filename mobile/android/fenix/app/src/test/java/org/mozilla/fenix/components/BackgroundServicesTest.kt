/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import io.mockk.MockKAnnotations
import io.mockk.Runs
import io.mockk.confirmVerified
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.just
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.service.nimbus.NimbusApi
import mozilla.components.support.base.observer.ObserverRegistry
import mozilla.components.support.test.robolectric.testContext
import mozilla.telemetry.glean.testing.GleanTestRule
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.SyncAuth
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.utils.Settings

// For gleanTestRule
@RunWith(FenixRobolectricTestRunner::class)
class BackgroundServicesTest {

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    @MockK
    private lateinit var context: Context

    @MockK
    private lateinit var settings: Settings

    @MockK
    private lateinit var nimbus: NimbusApi

    private lateinit var observer: TelemetryAccountObserver
    private lateinit var registry: ObserverRegistry<AccountObserver>

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        every { settings.signedInFxaAccount = any() } just Runs

        val mockComponents: Components = mockk()
        every { mockComponents.settings } returns settings
        every { mockComponents.analytics } returns mockk {
            every { experiments } returns nimbus
        }
        every { context.components } returns mockComponents
        every { nimbus.recordEvent(any()) } returns Unit

        observer = TelemetryAccountObserver(context)
        registry = ObserverRegistry<AccountObserver>().apply { register(observer) }
    }

    @Test
    fun `telemetry account observer tracks sign in event`() {
        val account = mockk<OAuthAccount>()

        registry.notifyObservers { onAuthenticated(account, AuthType.Signin) }
        assertEquals(1, SyncAuth.signIn.testGetValue()!!.size)
        assertEquals(null, SyncAuth.signIn.testGetValue()!!.single().extra)
        verify { settings.signedInFxaAccount = true }
        confirmVerified(settings)
    }

    @Test
    fun `telemetry account observer tracks sign up event`() {
        val account = mockk<OAuthAccount>()

        registry.notifyObservers { onAuthenticated(account, AuthType.Signup) }
        assertEquals(1, SyncAuth.signUp.testGetValue()!!.size)
        assertEquals(null, SyncAuth.signUp.testGetValue()!!.single().extra)
        verify { settings.signedInFxaAccount = true }
        confirmVerified(settings)
    }

    @Test
    fun `telemetry account observer tracks pairing event`() {
        val account = mockk<OAuthAccount>()

        registry.notifyObservers { onAuthenticated(account, AuthType.Pairing) }
        assertEquals(1, SyncAuth.paired.testGetValue()!!.size)
        assertEquals(null, SyncAuth.paired.testGetValue()!!.single().extra)
        verify { settings.signedInFxaAccount = true }
        confirmVerified(settings)
    }

    @Test
    fun `telemetry account observer tracks recovered event`() {
        val account = mockk<OAuthAccount>()

        registry.notifyObservers { onAuthenticated(account, AuthType.Recovered) }
        assertEquals(1, SyncAuth.recovered.testGetValue()!!.size)
        assertEquals(null, SyncAuth.recovered.testGetValue()!!.single().extra)
        verify { settings.signedInFxaAccount = true }
        confirmVerified(settings)
    }

    @Test
    fun `telemetry account observer tracks external creation event with null action`() {
        val account = mockk<OAuthAccount>()

        registry.notifyObservers { onAuthenticated(account, AuthType.OtherExternal(null)) }
        assertEquals(1, SyncAuth.otherExternal.testGetValue()!!.size)
        assertEquals(null, SyncAuth.otherExternal.testGetValue()!!.single().extra)
        verify { settings.signedInFxaAccount = true }
        confirmVerified(settings)
    }

    @Test
    fun `telemetry account observer tracks external creation event with some action`() {
        val account = mockk<OAuthAccount>()

        registry.notifyObservers { onAuthenticated(account, AuthType.OtherExternal("someAction")) }
        assertEquals(1, SyncAuth.otherExternal.testGetValue()!!.size)
        assertEquals(null, SyncAuth.otherExternal.testGetValue()!!.single().extra)
        verify { settings.signedInFxaAccount = true }
        confirmVerified(settings)
    }

    @Test
    fun `telemetry account observer tracks sign out event`() {
        registry.notifyObservers { onLoggedOut() }
        assertEquals(1, SyncAuth.signOut.testGetValue()!!.size)
        assertEquals(null, SyncAuth.signOut.testGetValue()!!.single().extra)
        verify { settings.signedInFxaAccount = false }
        confirmVerified(settings)
    }

    @Test
    fun `telemetry account observer records nimbus event for logins`() {
        observer.onAuthenticated(mockk(), AuthType.Signin)
        verify {
            nimbus.recordEvent("sync_auth_sign_in")
        }
        confirmVerified(nimbus)
    }
}
