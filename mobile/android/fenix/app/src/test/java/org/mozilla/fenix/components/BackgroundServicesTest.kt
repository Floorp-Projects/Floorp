/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import io.mockk.Called
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
import mozilla.components.support.base.observer.ObserverRegistry
import mozilla.components.support.test.robolectric.testContext
import mozilla.telemetry.glean.testing.GleanTestRule
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.SyncAuth
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.utils.Settings

// For gleanTestRule
@RunWith(FenixRobolectricTestRunner::class)
class BackgroundServicesTest {

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    @MockK
    private lateinit var metrics: MetricController

    @MockK
    private lateinit var settings: Settings

    private lateinit var observer: TelemetryAccountObserver
    private lateinit var registry: ObserverRegistry<AccountObserver>

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        every { metrics.track(any()) } just Runs
        every { settings.signedInFxaAccount = any() } just Runs

        observer = TelemetryAccountObserver(settings)
        registry = ObserverRegistry<AccountObserver>().apply { register(observer) }
    }

    @Test
    fun `telemetry account observer tracks sign in event`() {
        val account = mockk<OAuthAccount>()

        registry.notifyObservers { onAuthenticated(account, AuthType.Signin) }
        assertEquals(true, SyncAuth.signIn.testHasValue())
        assertEquals(1, SyncAuth.signIn.testGetValue().size)
        assertEquals(null, SyncAuth.signIn.testGetValue().single().extra)
        verify { settings.signedInFxaAccount = true }
        confirmVerified(metrics, settings)
    }

    @Test
    fun `telemetry account observer tracks sign up event`() {
        val account = mockk<OAuthAccount>()

        registry.notifyObservers { onAuthenticated(account, AuthType.Signup) }
        assertEquals(true, SyncAuth.signUp.testHasValue())
        assertEquals(1, SyncAuth.signUp.testGetValue().size)
        assertEquals(null, SyncAuth.signUp.testGetValue().single().extra)
        verify { settings.signedInFxaAccount = true }
        confirmVerified(metrics, settings)
    }

    @Test
    fun `telemetry account observer tracks pairing event`() {
        val account = mockk<OAuthAccount>()

        registry.notifyObservers { onAuthenticated(account, AuthType.Pairing) }
        assertEquals(true, SyncAuth.paired.testHasValue())
        assertEquals(1, SyncAuth.paired.testGetValue().size)
        assertEquals(null, SyncAuth.paired.testGetValue().single().extra)
        verify { settings.signedInFxaAccount = true }
        confirmVerified(metrics, settings)
    }

    @Test
    fun `telemetry account observer tracks shared copy event`() {
        val account = mockk<OAuthAccount>()

        registry.notifyObservers { onAuthenticated(account, AuthType.MigratedCopy) }
        verify { metrics wasNot Called }
        verify { settings.signedInFxaAccount = true }
        confirmVerified(metrics, settings)
    }

    @Test
    fun `telemetry account observer tracks shared reuse event`() {
        val account = mockk<OAuthAccount>()

        registry.notifyObservers { onAuthenticated(account, AuthType.MigratedReuse) }
        verify { metrics wasNot Called }
        verify { settings.signedInFxaAccount = true }
        confirmVerified(metrics, settings)
    }

    @Test
    fun `telemetry account observer tracks recovered event`() {
        val account = mockk<OAuthAccount>()

        registry.notifyObservers { onAuthenticated(account, AuthType.Recovered) }
        assertEquals(true, SyncAuth.recovered.testHasValue())
        assertEquals(1, SyncAuth.recovered.testGetValue().size)
        assertEquals(null, SyncAuth.recovered.testGetValue().single().extra)
        verify { settings.signedInFxaAccount = true }
        confirmVerified(metrics, settings)
    }

    @Test
    fun `telemetry account observer tracks external creation event with null action`() {
        val account = mockk<OAuthAccount>()

        registry.notifyObservers { onAuthenticated(account, AuthType.OtherExternal(null)) }
        assertEquals(true, SyncAuth.otherExternal.testHasValue())
        assertEquals(1, SyncAuth.otherExternal.testGetValue().size)
        assertEquals(null, SyncAuth.otherExternal.testGetValue().single().extra)
        verify { settings.signedInFxaAccount = true }
        confirmVerified(metrics, settings)
    }

    @Test
    fun `telemetry account observer tracks external creation event with some action`() {
        val account = mockk<OAuthAccount>()

        registry.notifyObservers { onAuthenticated(account, AuthType.OtherExternal("someAction")) }
        assertEquals(true, SyncAuth.otherExternal.testHasValue())
        assertEquals(1, SyncAuth.otherExternal.testGetValue().size)
        assertEquals(null, SyncAuth.otherExternal.testGetValue().single().extra)
        verify { settings.signedInFxaAccount = true }
        confirmVerified(metrics, settings)
    }

    @Test
    fun `telemetry account observer does not track existing account`() {
        val account = mockk<OAuthAccount>()

        registry.notifyObservers { onAuthenticated(account, AuthType.Existing) }
        verify { metrics wasNot Called }
        verify { settings.signedInFxaAccount = true }
        confirmVerified(metrics, settings)
    }

    @Test
    fun `telemetry account observer tracks sign out event`() {
        registry.notifyObservers { onLoggedOut() }
        assertEquals(true, SyncAuth.signOut.testHasValue())
        assertEquals(1, SyncAuth.signOut.testGetValue().size)
        assertEquals(null, SyncAuth.signOut.testGetValue().single().extra)
        verify { settings.signedInFxaAccount = false }
        confirmVerified(metrics, settings)
    }
}
