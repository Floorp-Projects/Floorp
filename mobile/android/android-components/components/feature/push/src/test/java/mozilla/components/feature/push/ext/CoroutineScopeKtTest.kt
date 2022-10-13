/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.feature.push.ext

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.appservices.push.InternalException
import mozilla.appservices.push.PushException.AlreadyRegisteredException
import mozilla.appservices.push.PushException.CommunicationException
import mozilla.appservices.push.PushException.CommunicationServerException
import mozilla.appservices.push.PushException.CryptoException
import mozilla.appservices.push.PushException.GeneralException
import mozilla.appservices.push.PushException.MissingRegistrationTokenException
import mozilla.appservices.push.PushException.RecordNotFoundException
import mozilla.appservices.push.PushException.StorageException
import mozilla.appservices.push.PushException.StorageSqlException
import mozilla.appservices.push.PushException.TranscodingException
import mozilla.appservices.push.PushException.UrlParseException
import mozilla.components.feature.push.exceptionHandler
import org.junit.Test

@ExperimentalCoroutinesApi
class CoroutineScopeKtTest {

    @Test(expected = InternalException::class)
    fun `launchAndTry throws on unrecoverable Rust exceptions`() = runTest {
        CoroutineScope(coroutineContext).launchAndTry(
            errorBlock = { throw InternalException("unit test") },
            block = { throw MissingRegistrationTokenException("") },
        )
    }

    @Test(expected = ArithmeticException::class)
    fun `launchAndTry throws original exception`() = runTest {
        CoroutineScope(coroutineContext).launchAndTry(
            errorBlock = { throw InternalException("unit test") },
            block = { throw ArithmeticException() },
        )
    }

    @Test
    fun `launchAndTry should NOT throw on recoverable Rust exceptions`() = runTest {
        CoroutineScope(coroutineContext).launchAndTry(
            { throw CryptoException("should not fail test") },
            { assert(true) },
        ) + exceptionHandler {}

        CoroutineScope(coroutineContext).launchAndTry(
            { throw CommunicationServerException("should not fail test") },
            { assert(true) },
        )

        CoroutineScope(coroutineContext).launchAndTry(
            { throw CommunicationException("should not fail test") },
            { assert(true) },
        )

        CoroutineScope(coroutineContext).launchAndTry(
            { throw AlreadyRegisteredException("") },
            { assert(true) },
        )

        CoroutineScope(coroutineContext).launchAndTry(
            { throw StorageException("should not fail test") },
            { assert(true) },
        )

        CoroutineScope(coroutineContext).launchAndTry(
            { throw MissingRegistrationTokenException("") },
            { assert(true) },
        )

        CoroutineScope(coroutineContext).launchAndTry(
            { throw StorageSqlException("should not fail test") },
            { assert(true) },
        )

        CoroutineScope(coroutineContext).launchAndTry(
            { throw TranscodingException("should not fail test") },
            { assert(true) },
        )

        CoroutineScope(coroutineContext).launchAndTry(
            { throw RecordNotFoundException("should not fail test") },
            { assert(true) },
        )

        CoroutineScope(coroutineContext).launchAndTry(
            { throw UrlParseException("should not fail test") },
            { assert(true) },
        )

        CoroutineScope(coroutineContext).launchAndTry(
            { throw GeneralException("should not fail test") },
            { assert(true) },
        )
    }
}
