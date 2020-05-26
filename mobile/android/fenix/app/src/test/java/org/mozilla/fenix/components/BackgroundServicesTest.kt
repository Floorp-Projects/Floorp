/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.service.fxa.DeviceConfig
import mozilla.components.service.fxa.ServerConfig
import mozilla.components.service.fxa.SyncConfig
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.support.base.observer.ObserverRegistry
import org.junit.Test
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController

class BackgroundServicesTest {
    class TestableBackgroundServices(
        val context: Context
    ) : BackgroundServices(context, mockk(), mockk(), mockk(), mockk(), mockk(), mockk()) {
        override fun makeAccountManager(
            context: Context,
            serverConfig: ServerConfig,
            deviceConfig: DeviceConfig,
            syncConfig: SyncConfig?
        ) = mockk<FxaAccountManager>(relaxed = true)
    }

    @Test
    fun `telemetry account observer`() {
        val metrics = mockk<MetricController>()
        every { metrics.track(any()) } just Runs
        val observer = TelemetryAccountObserver(mockk(relaxed = true), metrics)
        val registry = ObserverRegistry<AccountObserver>()
        registry.register(observer)
        val account = mockk<OAuthAccount>()

        // Sign-in
        registry.notifyObservers { onAuthenticated(account, AuthType.Signin) }
        verify(exactly = 1) { metrics.track(eq(Event.SyncAuthSignIn)) }
        verify(exactly = 0) { metrics.track(eq(Event.SyncAuthSignUp)) }
        verify(exactly = 0) { metrics.track(eq(Event.SyncAuthPaired)) }
        verify(exactly = 0) { metrics.track(eq(Event.SyncAuthFromShared)) }
        verify(exactly = 0) { metrics.track(eq(Event.SyncAuthRecovered)) }
        verify(exactly = 0) { metrics.track(eq(Event.SyncAuthOtherExternal)) }
        verify(exactly = 0) { metrics.track(eq(Event.SyncAuthSignOut)) }

        // Sign-up
        registry.notifyObservers { onAuthenticated(account, AuthType.Signup) }
        verify(exactly = 1) { metrics.track(eq(Event.SyncAuthSignIn)) }
        verify(exactly = 1) { metrics.track(eq(Event.SyncAuthSignUp)) }
        verify(exactly = 0) { metrics.track(eq(Event.SyncAuthPaired)) }
        verify(exactly = 0) { metrics.track(eq(Event.SyncAuthFromShared)) }
        verify(exactly = 0) { metrics.track(eq(Event.SyncAuthRecovered)) }
        verify(exactly = 0) { metrics.track(eq(Event.SyncAuthOtherExternal)) }
        verify(exactly = 0) { metrics.track(eq(Event.SyncAuthSignOut)) }

        // Pairing
        registry.notifyObservers { onAuthenticated(account, AuthType.Pairing) }
        verify(exactly = 1) { metrics.track(eq(Event.SyncAuthSignIn)) }
        verify(exactly = 1) { metrics.track(eq(Event.SyncAuthSignUp)) }
        verify(exactly = 1) { metrics.track(eq(Event.SyncAuthPaired)) }
        verify(exactly = 0) { metrics.track(eq(Event.SyncAuthFromShared)) }
        verify(exactly = 0) { metrics.track(eq(Event.SyncAuthRecovered)) }
        verify(exactly = 0) { metrics.track(eq(Event.SyncAuthOtherExternal)) }
        verify(exactly = 0) { metrics.track(eq(Event.SyncAuthSignOut)) }

        // Auto-login/shared account
        registry.notifyObservers { onAuthenticated(account, AuthType.Shared) }
        verify(exactly = 1) { metrics.track(eq(Event.SyncAuthSignIn)) }
        verify(exactly = 1) { metrics.track(eq(Event.SyncAuthSignUp)) }
        verify(exactly = 1) { metrics.track(eq(Event.SyncAuthPaired)) }
        verify(exactly = 1) { metrics.track(eq(Event.SyncAuthFromShared)) }
        verify(exactly = 0) { metrics.track(eq(Event.SyncAuthRecovered)) }
        verify(exactly = 0) { metrics.track(eq(Event.SyncAuthOtherExternal)) }
        verify(exactly = 0) { metrics.track(eq(Event.SyncAuthSignOut)) }

        // Internally recovered
        registry.notifyObservers { onAuthenticated(account, AuthType.Recovered) }
        verify(exactly = 1) { metrics.track(eq(Event.SyncAuthSignIn)) }
        verify(exactly = 1) { metrics.track(eq(Event.SyncAuthSignUp)) }
        verify(exactly = 1) { metrics.track(eq(Event.SyncAuthPaired)) }
        verify(exactly = 1) { metrics.track(eq(Event.SyncAuthFromShared)) }
        verify(exactly = 1) { metrics.track(eq(Event.SyncAuthRecovered)) }
        verify(exactly = 0) { metrics.track(eq(Event.SyncAuthOtherExternal)) }
        verify(exactly = 0) { metrics.track(eq(Event.SyncAuthSignOut)) }

        // Other external
        registry.notifyObservers { onAuthenticated(account, AuthType.OtherExternal(null)) }
        verify(exactly = 1) { metrics.track(eq(Event.SyncAuthSignIn)) }
        verify(exactly = 1) { metrics.track(eq(Event.SyncAuthSignUp)) }
        verify(exactly = 1) { metrics.track(eq(Event.SyncAuthPaired)) }
        verify(exactly = 1) { metrics.track(eq(Event.SyncAuthFromShared)) }
        verify(exactly = 1) { metrics.track(eq(Event.SyncAuthRecovered)) }
        verify(exactly = 1) { metrics.track(eq(Event.SyncAuthOtherExternal)) }
        verify(exactly = 0) { metrics.track(eq(Event.SyncAuthSignOut)) }

        registry.notifyObservers { onAuthenticated(account, AuthType.OtherExternal("someAction")) }
        verify(exactly = 1) { metrics.track(eq(Event.SyncAuthSignIn)) }
        verify(exactly = 1) { metrics.track(eq(Event.SyncAuthSignUp)) }
        verify(exactly = 1) { metrics.track(eq(Event.SyncAuthPaired)) }
        verify(exactly = 1) { metrics.track(eq(Event.SyncAuthFromShared)) }
        verify(exactly = 1) { metrics.track(eq(Event.SyncAuthRecovered)) }
        verify(exactly = 2) { metrics.track(eq(Event.SyncAuthOtherExternal)) }
        verify(exactly = 0) { metrics.track(eq(Event.SyncAuthSignOut)) }

        // NB: 'Existing' auth type isn't expected to record any auth telemetry.
        registry.notifyObservers { onAuthenticated(account, AuthType.Existing) }
        verify(exactly = 1) { metrics.track(eq(Event.SyncAuthSignIn)) }
        verify(exactly = 1) { metrics.track(eq(Event.SyncAuthSignUp)) }
        verify(exactly = 1) { metrics.track(eq(Event.SyncAuthPaired)) }
        verify(exactly = 1) { metrics.track(eq(Event.SyncAuthFromShared)) }
        verify(exactly = 1) { metrics.track(eq(Event.SyncAuthRecovered)) }
        verify(exactly = 2) { metrics.track(eq(Event.SyncAuthOtherExternal)) }
        verify(exactly = 0) { metrics.track(eq(Event.SyncAuthSignOut)) }

        // Logout
        registry.notifyObservers { onLoggedOut() }
        verify(exactly = 1) { metrics.track(eq(Event.SyncAuthSignIn)) }
        verify(exactly = 1) { metrics.track(eq(Event.SyncAuthSignUp)) }
        verify(exactly = 1) { metrics.track(eq(Event.SyncAuthPaired)) }
        verify(exactly = 1) { metrics.track(eq(Event.SyncAuthFromShared)) }
        verify(exactly = 1) { metrics.track(eq(Event.SyncAuthRecovered)) }
        verify(exactly = 2) { metrics.track(eq(Event.SyncAuthOtherExternal)) }
        verify(exactly = 1) { metrics.track(eq(Event.SyncAuthSignOut)) }
    }
}
