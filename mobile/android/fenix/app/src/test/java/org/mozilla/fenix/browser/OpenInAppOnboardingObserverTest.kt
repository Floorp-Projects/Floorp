/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.content.Context
import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.session.Session
import mozilla.components.feature.app.links.AppLinkRedirect
import mozilla.components.feature.app.links.AppLinksUseCases
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.utils.Settings

@ExperimentalCoroutinesApi
@RunWith(FenixRobolectricTestRunner::class)
class OpenInAppOnboardingObserverTest {

    @MockK(relaxed = true) private lateinit var context: Context
    @MockK(relaxed = true) private lateinit var settings: Settings
    @MockK(relaxed = true) private lateinit var session: Session
    @MockK(relaxed = true) private lateinit var appLinksUseCases: AppLinksUseCases
    @MockK(relaxed = true) private lateinit var applinksRedirect: AppLinkRedirect
    @MockK(relaxed = true) private lateinit var getAppLinkRedirect: AppLinksUseCases.GetAppLinkRedirect
    @MockK(relaxed = true) private lateinit var infoBanner: InfoBanner

    @Before
    fun setup() {
        MockKAnnotations.init(this)
    }

    @Test
    fun `do not show banner when openLinksInExternalApp is set to true`() {
        every { settings.openLinksInExternalApp } returns true
        every { settings.shouldShowOpenInAppCfr } returns true

        val observer = OpenInAppOnboardingObserver(context, mockk(), settings, appLinksUseCases, mockk())
        observer.onLoadingStateChanged(session, false)

        verify(exactly = 0) { appLinksUseCases.appLinkRedirect }

        every { settings.openLinksInExternalApp } returns false
        observer.onLoadingStateChanged(session, false)

        verify(exactly = 1) { appLinksUseCases.appLinkRedirect }
    }

    @Test
    fun `do not show banner when shouldShowOpenInAppCfr is set to false`() {
        every { settings.openLinksInExternalApp } returns false
        every { settings.shouldShowOpenInAppCfr } returns false

        val observer = OpenInAppOnboardingObserver(context, mockk(), settings, appLinksUseCases, mockk())
        observer.onLoadingStateChanged(session, false)

        verify(exactly = 0) { appLinksUseCases.appLinkRedirect }

        every { settings.shouldShowOpenInAppCfr } returns true
        observer.onLoadingStateChanged(session, false)

        verify(exactly = 1) { appLinksUseCases.appLinkRedirect }
    }

    @Test
    fun `do not show banner when URL is loading`() {
        every { settings.openLinksInExternalApp } returns false
        every { settings.shouldShowOpenInAppCfr } returns true

        val observer = OpenInAppOnboardingObserver(context, mockk(), settings, appLinksUseCases, mockk())

        observer.onLoadingStateChanged(session, true)

        verify(exactly = 0) { appLinksUseCases.appLinkRedirect }

        observer.onLoadingStateChanged(session, false)

        verify(exactly = 1) { appLinksUseCases.appLinkRedirect }
    }

    @Test
    fun `do not show banner when external app is not found`() {
        every { settings.openLinksInExternalApp } returns false
        every { settings.shouldShowOpenInAppCfr } returns true
        every { appLinksUseCases.appLinkRedirect } returns getAppLinkRedirect
        every { getAppLinkRedirect.invoke(any()) } returns applinksRedirect

        val observer = OpenInAppOnboardingObserver(context, mockk(), settings, appLinksUseCases, mockk())
        observer.onLoadingStateChanged(session, false)

        verify(exactly = 0) { settings.shouldShowOpenInAppBanner }
    }

    @Test
    fun `do not dismiss banner when URL is the same`() {
        val observer = OpenInAppOnboardingObserver(context, mockk(), settings, appLinksUseCases, mockk())
        observer.infoBanner = infoBanner
        observer.sessionDomainForDisplayedBanner = "mozilla.com"
        observer.onUrlChanged(session, "https://mozilla.com")

        verify(exactly = 0) { infoBanner.dismiss() }

        observer.onUrlChanged(session, "https://abc.com")
        verify(exactly = 1) { infoBanner.dismiss() }
    }
}
