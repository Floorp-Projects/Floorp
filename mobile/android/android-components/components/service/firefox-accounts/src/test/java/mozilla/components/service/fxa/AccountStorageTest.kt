/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.base.crash.CrashReporting
import mozilla.components.lib.dataprotect.SecureAbove22Preferences
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.reset
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoInteractions
import org.robolectric.annotation.Config
import kotlin.reflect.KClass

// Note that tests that involve secure storage specify API=21, because of issues testing secure storage on
// 23+ API levels. See https://github.com/mozilla-mobile/android-components/issues/4956

@RunWith(AndroidJUnit4::class)
class SharedPrefAccountStorageTest {
    @Config(sdk = [21])
    @Test
    fun `plain storage crud`() {
        val storage = SharedPrefAccountStorage(testContext)
        val account = FirefoxAccount(
            mozilla.appservices.fxaclient.Config(Server.RELEASE, "someId", "http://www.firefox.com")
        )
        assertNull(storage.read())
        storage.write(account.toJSONString())
        assertNotNull(storage.read())
        storage.clear()
        assertNull(storage.read())
    }

    @Config(sdk = [21])
    @Test
    fun `migration from SecureAbove22AccountStorage`() {
        val secureStorage = SecureAbove22AccountStorage(testContext)
        val account = FirefoxAccount(
            mozilla.appservices.fxaclient.Config(Server.RELEASE, "someId", "http://www.firefox.com")
        )

        assertNull(secureStorage.read())
        secureStorage.write(account.toJSONString())
        assertNotNull(secureStorage.read())

        // Now that we have account state in secureStorage, it should be migrated over to plainStorage when it's init'd.
        val plainStorage = SharedPrefAccountStorage(testContext)
        assertNotNull(plainStorage.read())
        // And secureStorage must have been cleared during this migration.
        assertNull(secureStorage.read())
    }

    @Config(sdk = [21])
    @Test
    fun `missing state is reported during a migration`() {
        val secureStorage = SecureAbove22AccountStorage(testContext)
        val account = FirefoxAccount(
            mozilla.appservices.fxaclient.Config(Server.RELEASE, "someId", "http://www.firefox.com")
        )
        secureStorage.write(account.toJSONString())

        // Clear the underlying storage layer "behind the back" of account storage.
        SecureAbove22Preferences(testContext, "fxaStateAC").clear()

        val crashReporter: CrashReporting = mock()
        val plainStorage = SharedPrefAccountStorage(testContext, crashReporter)
        assertCaughtException(crashReporter, AbnormalAccountStorageEvent.UnexpectedlyMissingAccountState::class)

        assertNull(plainStorage.read())

        reset(crashReporter)
        assertNull(secureStorage.read())
        verifyNoInteractions(crashReporter)
    }
}

@RunWith(AndroidJUnit4::class)
class SecureAbove22AccountStorageTest {
    @Config(sdk = [21])
    @Test
    fun `secure storage crud`() {
        val crashReporter: CrashReporting = mock()
        val storage = SecureAbove22AccountStorage(testContext, crashReporter)
        val account = FirefoxAccount(
            mozilla.appservices.fxaclient.Config(Server.RELEASE, "someId", "http://www.firefox.com")
        )
        assertNull(storage.read())
        storage.write(account.toJSONString())
        assertNotNull(storage.read())
        storage.clear()
        assertNull(storage.read())
        verifyNoInteractions(crashReporter)
    }

    @Config(sdk = [21])
    @Test
    fun `migration from SharedPrefAccountStorage`() {
        val plainStorage = SharedPrefAccountStorage(testContext)
        val account = FirefoxAccount(
            mozilla.appservices.fxaclient.Config(Server.RELEASE, "someId", "http://www.firefox.com")
        )

        assertNull(plainStorage.read())
        plainStorage.write(account.toJSONString())
        assertNotNull(plainStorage.read())

        // Now that we have account state in plainStorage, it should be migrated over to secureStorage when it's init'd.
        val crashReporter: CrashReporting = mock()
        val secureStorage = SecureAbove22AccountStorage(testContext, crashReporter)
        assertNotNull(secureStorage.read())
        // And plainStorage must have been cleared during this migration.
        assertNull(plainStorage.read())
        verifyNoInteractions(crashReporter)
    }

    @Config(sdk = [21])
    @Test
    fun `missing state is reported`() {
        val crashReporter: CrashReporting = mock()
        val storage = SecureAbove22AccountStorage(testContext, crashReporter)
        val account = FirefoxAccount(
            mozilla.appservices.fxaclient.Config(Server.RELEASE, "someId", "http://www.firefox.com")
        )
        storage.write(account.toJSONString())

        // Clear the underlying storage layer "behind the back" of account storage.
        SecureAbove22Preferences(testContext, "fxaStateAC").clear()
        assertNull(storage.read())
        assertCaughtException(crashReporter, AbnormalAccountStorageEvent.UnexpectedlyMissingAccountState::class)
        // Make sure exception is only reported once per "incident".
        reset(crashReporter)
        assertNull(storage.read())
        verifyNoInteractions(crashReporter)
    }

    @Config(sdk = [21])
    @Test
    fun `missing state is ignored without a configured crash reporter`() {
        val storage = SecureAbove22AccountStorage(testContext)
        val account = FirefoxAccount(
            mozilla.appservices.fxaclient.Config(Server.RELEASE, "someId", "http://www.firefox.com")
        )
        storage.write(account.toJSONString())

        // Clear the underlying storage layer "behind the back" of account storage.
        SecureAbove22Preferences(testContext, "fxaStateAC").clear()
        assertNull(storage.read())
    }
}

private fun <T : AbnormalAccountStorageEvent> assertCaughtException(crashReporter: CrashReporting, type: KClass<T>) {
    val captor = argumentCaptor<AbnormalAccountStorageEvent>()
    verify(crashReporter).submitCaughtException(captor.capture())
    Assert.assertEquals(type, captor.value::class)
}
