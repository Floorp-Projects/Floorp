/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.sync.logins

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.launch
import kotlinx.coroutines.test.TestCoroutineScope
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Before
import mozilla.components.concept.storage.Login
import mozilla.components.concept.storage.LoginsStorage
import mozilla.components.support.test.any
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.robolectric.annotation.Config

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class GeckoLoginStorageDelegateTest {

    private lateinit var loginsStorage: LoginsStorage
    private lateinit var delegate: GeckoLoginStorageDelegate
    private lateinit var scope: TestCoroutineScope

    @Before
    @Config(sdk = [21])
    fun before() {
        loginsStorage = mockLoginsStorage()
        scope = TestCoroutineScope()
        delegate = GeckoLoginStorageDelegate(lazy { loginsStorage }, { false }, scope)
    }

    @Test
    @Config(sdk = [21])
    fun `WHEN passed false for shouldAutofill onLoginsFetch returns early`() {
        scope.launch {
            delegate.onLoginFetch("login")
            verify(loginsStorage, times(0)).touch(any())
        }
    }

    @Test
    @Config(sdk = [21])
    fun `WHEN passed true for shouldAutofill onLoginsFetch does not return early`() {
        delegate = GeckoLoginStorageDelegate(lazy { loginsStorage }, { true }, scope)

        scope.launch {
            delegate.onLoginFetch("login")
            verify(loginsStorage, times(1)).touch(any())
        }
    }

    @Test
    @Config(sdk = [21])
    fun `WHEN onLoginsUsed is used THEN loginStorage should be touched`() {
        scope.launch {
            val login = createLogin("guid")

            delegate.onLoginUsed(login)
            verify(loginsStorage, times(1)).touch(any())
        }
    }

    @Test
    @Config(sdk = [21])
    fun `WHEN guid is null or empty THEN should create a new record`() {
        val serverPassword = createLogin()

        val fromNull = delegate.getPersistenceOperation(createLogin(guid = null), serverPassword)
        val fromEmpty = delegate.getPersistenceOperation(createLogin(guid = ""), serverPassword)

        assertEquals(Operation.CREATE, fromNull)
        assertEquals(Operation.CREATE, fromEmpty)
    }

    @Test
    @Config(sdk = [21])
    fun `WHEN guid matches existing record AND saved record has an empty username THEN should update existing record`() {
        val serverPassword = createLogin(guid = "1", username = "")
        val login = createLogin(guid = "1")

        assertEquals(Operation.UPDATE, delegate.getPersistenceOperation(login, serverPassword))
    }

    @Test
    @Config(sdk = [21])
    fun `WHEN guid matches existing record AND new username is different from saved THEN should create new record`() {
        val serverPassword = createLogin(guid = "1", username = "old")
        val login = createLogin(guid = "1", username = "new")

        assertEquals(Operation.CREATE, delegate.getPersistenceOperation(login, serverPassword))
    }

    @Test
    @Config(sdk = [21])
    fun `WHEN guid and username match THEN update existing record`() {
        val serverPassword = createLogin(guid = "1", username = "username")
        val login = createLogin(guid = "1", username = "username")

        assertEquals(Operation.UPDATE, delegate.getPersistenceOperation(login, serverPassword))
    }

    @Test
    @Config(sdk = [21])
    fun `GIVEN login is non-null, non-empty WHEN mergeWithLogin THEN result should use values from login`() {
        val login = Login(
            guid = "guid",
            origin = "origin",
            formActionOrigin = "fao",
            httpRealm = "httpRealm",
            username = "username",
            password = "password"
        )
        val serverPassword = createLogin(
            guid = "spId",
            origin = "spHost",
            username = "spUser",
            password = "spPassword",
            httpRealm = "spHttpRealm",
            formActionOrigin = "spFormSubmitUrl"
        )

        val expected = createLogin(
            guid = "spId",
            origin = "origin",
            username = "username",
            password = "password",
            httpRealm = "httpRealm",
            formActionOrigin = "fao"
        )

        assertEquals(expected, serverPassword.mergeWithLogin(login))
    }

    @Test
    @Config(sdk = [21])
    fun `GIVEN login has null values WHEN mergeWithLogin THEN those values should be taken from serverPassword`() {
        val login = Login(
            guid = null,
            origin = "origin",
            formActionOrigin = null,
            httpRealm = null,
            username = "username",
            password = "password"
        )
        val serverPassword = createLogin(
            guid = "spId",
            origin = "spHost",
            username = "spUser",
            password = "spPassword",
            httpRealm = "spHttpRealm",
            formActionOrigin = "spFormSubmitUrl"
        )

        val expected = createLogin(
            guid = "spId",
            origin = "origin",
            username = "username",
            password = "password",
            httpRealm = "spHttpRealm",
            formActionOrigin = "spFormSubmitUrl"
        )

        assertEquals(expected, serverPassword.mergeWithLogin(login))
    }

    @Test
    @Config(sdk = [21])
    fun `GIVEN login has empty values WHEN mergeWithLogin THEN those values should be taken from serverPassword`() {
        val login = Login(
            guid = "",
            origin = "",
            formActionOrigin = "",
            httpRealm = "",
            username = "",
            password = ""
        )
        val serverPassword = createLogin(
            guid = "spId",
            origin = "spHost",
            username = "spUser",
            password = "spPassword",
            httpRealm = "spHttpRealm",
            formActionOrigin = "spFormSubmitUrl"
        )

        val expected = createLogin(
            guid = "spId",
            origin = "spHost",
            username = "spUser",
            password = "spPassword",
            httpRealm = "spHttpRealm",
            formActionOrigin = "spFormSubmitUrl"
        )

        assertEquals(expected, serverPassword.mergeWithLogin(login))
    }
}

fun mockLoginsStorage(): LoginsStorage {
    val loginsStorage = mock<LoginsStorage>()
    return loginsStorage
}

fun createLogin(guid: String?, username: String = "username") = Login(
    guid = guid,
    username = username,
    password = "password",
    origin = "origin"
)

fun createLogin(
    guid: String = "id",
    password: String = "password",
    username: String = "username",
    origin: String = "hostname",
    httpRealm: String = "httpRealm",
    formActionOrigin: String = "formsubmiturl",
    usernameField: String = "usernameField",
    passwordField: String = "passwordField"

) = Login(
    guid = guid,
    origin = origin,
    password = password,
    username = username,
    httpRealm = httpRealm,
    formActionOrigin = formActionOrigin,
    usernameField = usernameField,
    passwordField = passwordField
)
