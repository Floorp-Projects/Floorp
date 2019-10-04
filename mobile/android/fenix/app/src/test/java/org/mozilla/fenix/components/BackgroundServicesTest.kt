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
import mozilla.components.feature.push.AutoPushFeature
import mozilla.components.feature.push.PushConfig
import mozilla.components.feature.push.PushType
import mozilla.components.service.fxa.DeviceConfig
import mozilla.components.service.fxa.ServerConfig
import mozilla.components.service.fxa.SyncConfig
import mozilla.components.service.fxa.SyncEngine
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.support.base.observer.ObserverRegistry
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import org.mozilla.fenix.Experiments
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.isInExperiment

class BackgroundServicesTest {
    class TestableBackgroundServices(
        val context: Context
    ) : BackgroundServices(context, mockk(), mockk()) {
        override fun makeAccountManager(
            context: Context,
            serverConfig: ServerConfig,
            deviceConfig: DeviceConfig,
            syncConfig: SyncConfig?
        ) = mockk<FxaAccountManager>(relaxed = true)

        override fun makePushConfig() = mockk<PushConfig>(relaxed = true)
        override fun makePush(pushConfig: PushConfig) = mockk<AutoPushFeature>(relaxed = true)
    }

    @Test
    fun `experiment flags`() {
        val context = mockk<Context>(relaxed = true)

        every { context.isInExperiment(eq(Experiments.asFeatureWebChannelsDisabled)) } returns false
        assertEquals("urn:ietf:wg:oauth:2.0:oob:oauth-redirect-webchannel", FxaServer.redirectUrl(context))

        every { context.isInExperiment(eq(Experiments.asFeatureWebChannelsDisabled)) } returns true
        assertEquals("https://accounts.firefox.com/oauth/success/a2270f727f45f648", FxaServer.redirectUrl(context))

        every { context.isInExperiment(eq(Experiments.asFeatureSyncDisabled)) } returns false
        var backgroundServices = TestableBackgroundServices(context)
        assertEquals(
            SyncConfig(setOf(SyncEngine.History, SyncEngine.Bookmarks), syncPeriodInMinutes = 240L),
            backgroundServices.syncConfig
        )

        every { context.isInExperiment(eq(Experiments.asFeatureSyncDisabled)) } returns true
        backgroundServices = TestableBackgroundServices(context)
        assertNull(backgroundServices.syncConfig)
    }

    @Test
    fun `push account observer`() {
        val push = mockk<AutoPushFeature>()
        val observer = PushAccountObserver(push)
        val registry = ObserverRegistry<AccountObserver>()
        registry.register(observer)
        val account = mockk<OAuthAccount>()

        // Being explicit here (vs using 'any()') ensures that any change to which PushType variants
        // are being subscribed/unsubscribed will break these tests, forcing developer to expand them.
        every { push.subscribeForType(PushType.Services) } just Runs
        every { push.unsubscribeForType(PushType.Services) } just Runs

        // 'Existing' auth type doesn't trigger subscription - we're already subscribed.
        registry.notifyObservers { onAuthenticated(account, AuthType.Existing) }
        verify(exactly = 0) { push.subscribeForType(any()) }

        // Every other auth type does.
        registry.notifyObservers { onAuthenticated(account, AuthType.Signin) }
        verify(exactly = 1) { push.subscribeForType(eq(PushType.Services)) }

        registry.notifyObservers { onAuthenticated(account, AuthType.Signup) }
        verify(exactly = 2) { push.subscribeForType(eq(PushType.Services)) }

        registry.notifyObservers { onAuthenticated(account, AuthType.Recovered) }
        verify(exactly = 3) { push.subscribeForType(eq(PushType.Services)) }

        registry.notifyObservers { onAuthenticated(account, AuthType.Shared) }
        verify(exactly = 4) { push.subscribeForType(eq(PushType.Services)) }

        registry.notifyObservers { onAuthenticated(account, AuthType.Pairing) }
        verify(exactly = 5) { push.subscribeForType(eq(PushType.Services)) }

        registry.notifyObservers { onAuthenticated(account, AuthType.OtherExternal(null)) }
        verify(exactly = 6) { push.subscribeForType(eq(PushType.Services)) }

        registry.notifyObservers { onAuthenticated(account, AuthType.OtherExternal("someAction")) }
        verify(exactly = 7) { push.subscribeForType(eq(PushType.Services)) }

        // None of the above unsubscribed.
        verify(exactly = 0) { push.unsubscribeForType(any()) }

        // Finally, log-out should unsubscribe.
        registry.notifyObservers { onLoggedOut() }
        verify(exactly = 1) { push.unsubscribeForType(eq(PushType.Services)) }
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
