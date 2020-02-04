/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa.manager

import mozilla.components.concept.sync.AuthException
import mozilla.components.concept.sync.AuthExceptionType
import mozilla.components.concept.sync.AuthType
import mozilla.components.service.fxa.FxaAuthData
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class FxaStateMatrixTest {
    @Test
    fun `state transitions`() {
        // State 'Start'.
        var state = AccountState.Start

        assertEquals(AccountState.Start, FxaStateMatrix.nextState(state, Event.Init))
        assertEquals(AccountState.NotAuthenticated, FxaStateMatrix.nextState(state, Event.AccountNotFound))
        assertEquals(AccountState.AuthenticatedNoProfile, FxaStateMatrix.nextState(state, Event.AccountRestored))
        assertNull(FxaStateMatrix.nextState(state, Event.Authenticate))
        assertNull(FxaStateMatrix.nextState(state, Event.Authenticated(FxaAuthData(AuthType.Signin, "code", "state"))))
        assertNull(FxaStateMatrix.nextState(state, Event.FetchProfile))
        assertNull(FxaStateMatrix.nextState(state, Event.FetchedProfile))
        assertNull(FxaStateMatrix.nextState(state, Event.FailedToFetchProfile))
        assertNull(FxaStateMatrix.nextState(state, Event.FailedToAuthenticate))
        assertNull(FxaStateMatrix.nextState(state, Event.Logout))
        assertNull(FxaStateMatrix.nextState(state, Event.AuthenticationError(AuthException(AuthExceptionType.UNAUTHORIZED))))
        assertNull(FxaStateMatrix.nextState(state, Event.SignInShareableAccount(mock(), false)))
        assertNull(FxaStateMatrix.nextState(state, Event.SignedInShareableAccount(false)))
        assertEquals(AccountState.CanAutoRetryAuthenticationViaTokenReuse, FxaStateMatrix.nextState(state, Event.InFlightReuseMigration))
        assertEquals(AccountState.CanAutoRetryAuthenticationViaTokenCopy, FxaStateMatrix.nextState(state, Event.InFlightCopyMigration))
        assertNull(FxaStateMatrix.nextState(state, Event.RetryLaterViaTokenReuse))
        assertNull(FxaStateMatrix.nextState(state, Event.RetryLaterViaTokenCopy))
        assertNull(FxaStateMatrix.nextState(state, Event.RetryMigration))

        // State 'NotAuthenticated'.
        state = AccountState.NotAuthenticated
        assertNull(FxaStateMatrix.nextState(state, Event.Init))
        assertNull(FxaStateMatrix.nextState(state, Event.AccountNotFound))
        assertNull(FxaStateMatrix.nextState(state, Event.AccountRestored))
        assertEquals(AccountState.NotAuthenticated, FxaStateMatrix.nextState(state, Event.Authenticate))
        assertEquals(AccountState.NotAuthenticated, FxaStateMatrix.nextState(state, Event.Pair("auth://pair")))
        assertEquals(AccountState.AuthenticatedNoProfile, FxaStateMatrix.nextState(state, Event.Authenticated(FxaAuthData(AuthType.Signup, "code", "state"))))
        assertNull(FxaStateMatrix.nextState(state, Event.FetchProfile))
        assertNull(FxaStateMatrix.nextState(state, Event.FetchedProfile))
        assertNull(FxaStateMatrix.nextState(state, Event.FailedToFetchProfile))
        assertEquals(AccountState.NotAuthenticated, FxaStateMatrix.nextState(state, Event.FailedToAuthenticate))
        assertNull(FxaStateMatrix.nextState(state, Event.Logout))
        assertNull(FxaStateMatrix.nextState(state, Event.AuthenticationError(AuthException(AuthExceptionType.UNAUTHORIZED))))
        assertEquals(AccountState.NotAuthenticated, FxaStateMatrix.nextState(state, Event.SignInShareableAccount(mock(), false)))
        assertEquals(AccountState.AuthenticatedNoProfile, FxaStateMatrix.nextState(state, Event.SignedInShareableAccount(false)))
        assertNull(FxaStateMatrix.nextState(state, Event.InFlightReuseMigration))
        assertNull(FxaStateMatrix.nextState(state, Event.InFlightCopyMigration))
        assertEquals(AccountState.CanAutoRetryAuthenticationViaTokenReuse, FxaStateMatrix.nextState(state, Event.RetryLaterViaTokenReuse))
        assertEquals(AccountState.CanAutoRetryAuthenticationViaTokenCopy, FxaStateMatrix.nextState(state, Event.RetryLaterViaTokenCopy))
        assertNull(FxaStateMatrix.nextState(state, Event.RetryMigration))

        // State 'AuthenticatedNoProfile'.
        state = AccountState.AuthenticatedNoProfile
        assertNull(FxaStateMatrix.nextState(state, Event.Init))
        assertNull(FxaStateMatrix.nextState(state, Event.AccountNotFound))
        assertNull(FxaStateMatrix.nextState(state, Event.AccountRestored))
        assertNull(FxaStateMatrix.nextState(state, Event.Authenticate))
        assertNull(FxaStateMatrix.nextState(state, Event.Authenticated(FxaAuthData(AuthType.Pairing, "code", "state"))))
        assertEquals(AccountState.AuthenticatedNoProfile, FxaStateMatrix.nextState(state, Event.FetchProfile))
        assertEquals(AccountState.AuthenticatedWithProfile, FxaStateMatrix.nextState(state, Event.FetchedProfile))
        assertEquals(AccountState.AuthenticatedNoProfile, FxaStateMatrix.nextState(state, Event.FailedToFetchProfile))
        assertNull(FxaStateMatrix.nextState(state, Event.FailedToAuthenticate))
        assertEquals(AccountState.NotAuthenticated, FxaStateMatrix.nextState(state, Event.Logout))
        assertEquals(AccountState.AuthenticationProblem, FxaStateMatrix.nextState(state, Event.AuthenticationError(AuthException(AuthExceptionType.UNAUTHORIZED))))
        assertNull(FxaStateMatrix.nextState(state, Event.SignInShareableAccount(mock(), false)))
        assertNull(FxaStateMatrix.nextState(state, Event.SignedInShareableAccount(false)))
        assertNull(FxaStateMatrix.nextState(state, Event.InFlightReuseMigration))
        assertNull(FxaStateMatrix.nextState(state, Event.InFlightCopyMigration))
        assertNull(FxaStateMatrix.nextState(state, Event.RetryLaterViaTokenReuse))
        assertNull(FxaStateMatrix.nextState(state, Event.RetryLaterViaTokenCopy))
        assertNull(FxaStateMatrix.nextState(state, Event.RetryMigration))

        // State 'AuthenticatedWithProfile'.
        state = AccountState.AuthenticatedWithProfile
        assertNull(FxaStateMatrix.nextState(state, Event.Init))
        assertNull(FxaStateMatrix.nextState(state, Event.AccountNotFound))
        assertNull(FxaStateMatrix.nextState(state, Event.AccountRestored))
        assertNull(FxaStateMatrix.nextState(state, Event.Authenticate))
        assertNull(FxaStateMatrix.nextState(state, Event.Authenticated(FxaAuthData(AuthType.OtherExternal("oi"), "code", "state"))))
        assertNull(FxaStateMatrix.nextState(state, Event.FetchProfile))
        assertNull(FxaStateMatrix.nextState(state, Event.FetchedProfile))
        assertNull(FxaStateMatrix.nextState(state, Event.FailedToFetchProfile))
        assertNull(FxaStateMatrix.nextState(state, Event.FailedToAuthenticate))
        assertEquals(AccountState.NotAuthenticated, FxaStateMatrix.nextState(state, Event.Logout))
        assertEquals(AccountState.AuthenticationProblem, FxaStateMatrix.nextState(state, Event.AuthenticationError(AuthException(AuthExceptionType.UNAUTHORIZED))))
        assertNull(FxaStateMatrix.nextState(state, Event.SignInShareableAccount(mock(), false)))
        assertNull(FxaStateMatrix.nextState(state, Event.SignedInShareableAccount(false)))
        assertNull(FxaStateMatrix.nextState(state, Event.InFlightReuseMigration))
        assertNull(FxaStateMatrix.nextState(state, Event.InFlightCopyMigration))
        assertNull(FxaStateMatrix.nextState(state, Event.RetryLaterViaTokenReuse))
        assertNull(FxaStateMatrix.nextState(state, Event.RetryLaterViaTokenCopy))
        assertNull(FxaStateMatrix.nextState(state, Event.RetryMigration))

        // State 'AuthenticationProblem'.
        state = AccountState.AuthenticationProblem
        assertNull(FxaStateMatrix.nextState(state, Event.Init))
        assertNull(FxaStateMatrix.nextState(state, Event.AccountNotFound))
        assertNull(FxaStateMatrix.nextState(state, Event.AccountRestored))
        assertEquals(AccountState.AuthenticationProblem, FxaStateMatrix.nextState(state, Event.Authenticate))
        assertEquals(AccountState.AuthenticatedNoProfile, FxaStateMatrix.nextState(state, Event.Authenticated(FxaAuthData(AuthType.Signin, "code", "state"))))
        assertNull(FxaStateMatrix.nextState(state, Event.FetchProfile))
        assertNull(FxaStateMatrix.nextState(state, Event.FetchedProfile))
        assertNull(FxaStateMatrix.nextState(state, Event.FailedToFetchProfile))
        assertEquals(AccountState.AuthenticationProblem, FxaStateMatrix.nextState(state, Event.FailedToAuthenticate))
        assertEquals(AccountState.NotAuthenticated, FxaStateMatrix.nextState(state, Event.Logout))
        assertNull(FxaStateMatrix.nextState(state, Event.AuthenticationError(AuthException(AuthExceptionType.UNAUTHORIZED))))
        assertNull(FxaStateMatrix.nextState(state, Event.SignInShareableAccount(mock(), false)))
        assertNull(FxaStateMatrix.nextState(state, Event.SignedInShareableAccount(false)))
        assertNull(FxaStateMatrix.nextState(state, Event.InFlightReuseMigration))
        assertNull(FxaStateMatrix.nextState(state, Event.InFlightCopyMigration))
        assertNull(FxaStateMatrix.nextState(state, Event.RetryLaterViaTokenReuse))
        assertNull(FxaStateMatrix.nextState(state, Event.RetryLaterViaTokenCopy))
        assertNull(FxaStateMatrix.nextState(state, Event.RetryMigration))

        // State 'CanAutoRetryAuthenticationViaTokenReuse'.
        state = AccountState.CanAutoRetryAuthenticationViaTokenReuse
        assertNull(FxaStateMatrix.nextState(state, Event.Init))
        assertNull(FxaStateMatrix.nextState(state, Event.AccountNotFound))
        assertNull(FxaStateMatrix.nextState(state, Event.AccountRestored))
        assertNull(FxaStateMatrix.nextState(state, Event.Authenticate))
        assertNull(FxaStateMatrix.nextState(state, Event.Authenticated(FxaAuthData(AuthType.Signin, "code", "state"))))
        assertNull(FxaStateMatrix.nextState(state, Event.FetchProfile))
        assertNull(FxaStateMatrix.nextState(state, Event.FetchedProfile))
        assertNull(FxaStateMatrix.nextState(state, Event.FailedToFetchProfile))
        assertEquals(AccountState.NotAuthenticated, FxaStateMatrix.nextState(state, Event.FailedToAuthenticate))
        assertEquals(AccountState.NotAuthenticated, FxaStateMatrix.nextState(state, Event.Logout))
        assertNull(FxaStateMatrix.nextState(state, Event.AuthenticationError(AuthException(AuthExceptionType.UNAUTHORIZED))))
        assertNull(FxaStateMatrix.nextState(state, Event.SignInShareableAccount(mock(), false)))
        assertEquals(AccountState.AuthenticatedNoProfile, FxaStateMatrix.nextState(state, Event.SignedInShareableAccount(false)))
        assertNull(FxaStateMatrix.nextState(state, Event.InFlightReuseMigration))
        assertNull(FxaStateMatrix.nextState(state, Event.InFlightCopyMigration))
        assertEquals(AccountState.CanAutoRetryAuthenticationViaTokenReuse, FxaStateMatrix.nextState(state, Event.RetryLaterViaTokenReuse))
        assertNull(FxaStateMatrix.nextState(state, Event.RetryLaterViaTokenCopy))
        assertEquals(AccountState.CanAutoRetryAuthenticationViaTokenReuse, FxaStateMatrix.nextState(state, Event.RetryMigration))

        // State 'CanAutoRetryAuthenticationViaTokenCopy'.
        state = AccountState.CanAutoRetryAuthenticationViaTokenCopy
        assertNull(FxaStateMatrix.nextState(state, Event.Init))
        assertNull(FxaStateMatrix.nextState(state, Event.AccountNotFound))
        assertNull(FxaStateMatrix.nextState(state, Event.AccountRestored))
        assertNull(FxaStateMatrix.nextState(state, Event.Authenticate))
        assertNull(FxaStateMatrix.nextState(state, Event.Authenticated(FxaAuthData(AuthType.Signin, "code", "state"))))
        assertNull(FxaStateMatrix.nextState(state, Event.FetchProfile))
        assertNull(FxaStateMatrix.nextState(state, Event.FetchedProfile))
        assertNull(FxaStateMatrix.nextState(state, Event.FailedToFetchProfile))
        assertEquals(AccountState.NotAuthenticated, FxaStateMatrix.nextState(state, Event.FailedToAuthenticate))
        assertEquals(AccountState.NotAuthenticated, FxaStateMatrix.nextState(state, Event.Logout))
        assertNull(FxaStateMatrix.nextState(state, Event.AuthenticationError(AuthException(AuthExceptionType.UNAUTHORIZED))))
        assertNull(FxaStateMatrix.nextState(state, Event.SignInShareableAccount(mock(), false)))
        assertEquals(AccountState.AuthenticatedNoProfile, FxaStateMatrix.nextState(state, Event.SignedInShareableAccount(false)))
        assertNull(FxaStateMatrix.nextState(state, Event.InFlightReuseMigration))
        assertNull(FxaStateMatrix.nextState(state, Event.InFlightCopyMigration))
        assertNull(FxaStateMatrix.nextState(state, Event.RetryLaterViaTokenReuse))
        assertEquals(AccountState.CanAutoRetryAuthenticationViaTokenCopy, FxaStateMatrix.nextState(state, Event.RetryLaterViaTokenCopy))
        assertEquals(AccountState.CanAutoRetryAuthenticationViaTokenCopy, FxaStateMatrix.nextState(state, Event.RetryMigration))
    }
}
