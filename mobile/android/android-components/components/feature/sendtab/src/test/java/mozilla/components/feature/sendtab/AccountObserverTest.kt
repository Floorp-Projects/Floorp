/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.feature.sendtab

import mozilla.components.feature.push.AutoPushFeature
import mozilla.components.feature.push.PushType
import mozilla.components.support.test.mock
import org.junit.Test
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions

class AccountObserverTest {
    @Test
    fun `feature and service invoked on new account authenticated`() {
        val feature: AutoPushFeature = mock()
        val observer = AccountObserver(feature)

        observer.onAuthenticated(mock(), true)

        verify(feature).subscribeForType(PushType.Services)

        verifyNoMoreInteractions(feature)
    }

    @Test
    fun `feature and service are not invoked if not provided`() {
        val feature: AutoPushFeature = mock()
        val observer = AccountObserver(null)

        observer.onAuthenticated(mock(), true)
        observer.onLoggedOut()

        verifyNoMoreInteractions(feature)
    }

    @Test
    fun `feature does not subscribe if not a new account`() {
        val feature: AutoPushFeature = mock()
        val observer = AccountObserver(feature)

        observer.onAuthenticated(mock(), false)

        verifyNoMoreInteractions(feature)
    }

    @Test
    fun `feature and service invoked on logout`() {
        val feature: AutoPushFeature = mock()
        val observer = AccountObserver(feature)

        observer.onLoggedOut()

        verify(feature).unsubscribeForType(PushType.Services)

        verifyNoMoreInteractions(feature)
    }

    @Test
    fun `feature and service not invoked for any other callback`() {
        val feature: AutoPushFeature = mock()
        val observer = AccountObserver(feature)

        observer.onAuthenticationProblems()
        observer.onProfileUpdated(mock())

        verifyNoMoreInteractions(feature)
    }
}