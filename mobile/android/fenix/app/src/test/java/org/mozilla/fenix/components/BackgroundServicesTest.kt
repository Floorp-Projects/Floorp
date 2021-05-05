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
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.utils.Settings

class BackgroundServicesTest {

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

        observer = TelemetryAccountObserver(settings, metrics)
        registry = ObserverRegistry<AccountObserver>().apply { register(observer) }
    }

    @Test
    fun `telemetry account observer tracks sign in event`() {
        val account = mockk<OAuthAccount>()

        registry.notifyObservers { onAuthenticated(account, AuthType.Signin) }
        verify { metrics.track(Event.SyncAuthSignIn) }
        verify { settings.signedInFxaAccount = true }
        confirmVerified(metrics, settings)
    }

    @Test
    fun `telemetry account observer tracks sign up event`() {
        val account = mockk<OAuthAccount>()

        registry.notifyObservers { onAuthenticated(account, AuthType.Signup) }
        verify { metrics.track(Event.SyncAuthSignUp) }
        verify { settings.signedInFxaAccount = true }
        confirmVerified(metrics, settings)
    }

    @Test
    fun `telemetry account observer tracks pairing event`() {
        val account = mockk<OAuthAccount>()

        registry.notifyObservers { onAuthenticated(account, AuthType.Pairing) }
        verify { metrics.track(Event.SyncAuthPaired) }
        verify { settings.signedInFxaAccount = true }
        confirmVerified(metrics, settings)
    }

    @Test
    fun `telemetry account observer tracks shared event`() {
        val account = mockk<OAuthAccount>()

        registry.notifyObservers { onAuthenticated(account, AuthType.MigratedReuse) }
        verify { metrics.track(Event.SyncAuthFromSharedReuse) }
        verify { settings.signedInFxaAccount = true }
        confirmVerified(metrics, settings)

        registry.notifyObservers { onAuthenticated(account, AuthType.MigratedCopy) }
        verify { metrics.track(Event.SyncAuthFromSharedCopy) }
    }

    @Test
    fun `telemetry account observer tracks recovered event`() {
        val account = mockk<OAuthAccount>()

        registry.notifyObservers { onAuthenticated(account, AuthType.Recovered) }
        verify { metrics.track(Event.SyncAuthRecovered) }
        verify { settings.signedInFxaAccount = true }
        confirmVerified(metrics, settings)
    }

    @Test
    fun `telemetry account observer tracks external creation event with null action`() {
        val account = mockk<OAuthAccount>()

        registry.notifyObservers { onAuthenticated(account, AuthType.OtherExternal(null)) }
        verify { metrics.track(Event.SyncAuthOtherExternal) }
        verify { settings.signedInFxaAccount = true }
        confirmVerified(metrics, settings)
    }

    @Test
    fun `telemetry account observer tracks external creation event with some action`() {
        val account = mockk<OAuthAccount>()

        registry.notifyObservers { onAuthenticated(account, AuthType.OtherExternal("someAction")) }
        verify { metrics.track(Event.SyncAuthOtherExternal) }
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
        verify { metrics.track(Event.SyncAuthSignOut) }
        verify { settings.signedInFxaAccount = false }
        confirmVerified(metrics, settings)
    }
}
