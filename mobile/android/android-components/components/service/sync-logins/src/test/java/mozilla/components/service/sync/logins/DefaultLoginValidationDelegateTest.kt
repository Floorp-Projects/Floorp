/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.sync.logins

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.test.TestCoroutineScope
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.concept.storage.Login
import mozilla.components.concept.storage.LoginValidationDelegate.Result
import mozilla.components.service.sync.logins.DefaultLoginValidationDelegate.Companion.mergeWithLogin
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
@ExperimentalCoroutinesApi
class DefaultLoginValidationDelegateTest {

    private lateinit var validationDelegate: DefaultLoginValidationDelegate
    private lateinit var scope: TestCoroutineScope

    private val loginsStorage = SyncableLoginsStorage(testContext, "dummy-key")

    init {
        // We need to ensure that the db directory is present - apparently, it's not by default in a
        // test environment, otherwise tests below will fail since `SyncableLoginsStorage` assumes
        // this directory exists. Note that on actual devices, this doesn't seem to be a problem we've
        // observed so far.
        testContext.getDatabasePath(DB_NAME)!!.parentFile!!.mkdirs()
    }

    @Before
    fun before() = runBlocking {
        loginsStorage.wipeLocal()
        scope = TestCoroutineScope()
        validationDelegate =
            DefaultLoginValidationDelegate(storage = lazy { loginsStorage }, scope = scope)
    }

    @Test
    fun `WHEN potential dupes list is empty, should create`() = runBlocking {
        val newLogin = createLogin(guid = "1")
        val operation = validationDelegate.shouldUpdateOrCreateAsync(newLogin).await()
        assertEquals(Result.CanBeCreated, operation)
    }

    @Test
    fun `WHEN existing login matches by guid and username, should update`() = runBlocking {
        val newLogin = createLogin(guid = "1")
        val existingLogin = createLogin(
            guid = "1",
            password = "password1"
        )
        loginsStorage.addWithGuid(existingLogin)
        val operation = validationDelegate.shouldUpdateOrCreateAsync(newLogin).await()
        assertEquals(Result.CanBeUpdated(loginsStorage.get("1")!!), operation)
    }

    @Test
    fun `WHEN multiple existing dupes, should update matching one with matching GUID and username first`() =
        runBlocking {
            val newLogin = createLogin(guid = "1")
            loginsStorage.addWithGuid(newLogin)
            val matchingLoginByUsername = createLogin(guid = "2", username = "")
            loginsStorage.addWithGuid(matchingLoginByUsername)
            val operation = validationDelegate.shouldUpdateOrCreateAsync(newLogin).await()
            assertEquals(Result.CanBeUpdated(loginsStorage.get("1")!!), operation)
        }

    @Test
    fun `WHEN saved login GUID matches, and is missing a username, should update`() =
        runBlocking {
            val newLogin = createLogin(guid = "1")
            val existingLogin = createLogin(
                guid = "1",
                username = ""
            )
            loginsStorage.addWithGuid(existingLogin)
            val operation = validationDelegate.shouldUpdateOrCreateAsync(newLogin).await()
            assertEquals(Result.CanBeUpdated(loginsStorage.get("1")!!), operation)
        }

    @Test
    fun `WHEN existing login matches by username but not GUID, should update`() = runBlocking {
        val newLogin = createLogin(guid = "1")
        val savedLogin = createLogin(guid = "2")
        loginsStorage.addWithGuid(savedLogin)
        val operation = validationDelegate.shouldUpdateOrCreateAsync(newLogin)
        assertEquals(Result.CanBeUpdated(loginsStorage.get("2")!!), operation.await())
    }

    @Test
    fun `WHEN existing login has blank username and doesn't match by guid, should update`() =
        runBlocking {
            val newLogin = createLogin(guid = "1")
            val savedLogin =
                createLogin(guid = "2", username = "")
            loginsStorage.addWithGuid(savedLogin)
            val operation = validationDelegate.shouldUpdateOrCreateAsync(newLogin).await()
            assertEquals(Result.CanBeUpdated(loginsStorage.get("2")!!), operation)
        }

    @Test
    fun `WHEN saved login GUID matches, but username is updated, we should create`() =
        runBlocking {
            val newLogin = createLogin(guid = "1")
            val existingLogin = createLogin(
                guid = "1",
                username = "username2"
            )
            loginsStorage.addWithGuid(existingLogin)
            val operation = validationDelegate.shouldUpdateOrCreateAsync(newLogin).await()
            assertEquals(Result.CanBeCreated, operation)
        }

    @Test
    fun `GIVEN login is non-null, non-empty WHEN mergeWithLogin THEN result should use values from login`() =
        runBlockingTest {
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
    fun `GIVEN login has null values WHEN mergeWithLogin THEN those values should be taken from serverPassword`() =
        runBlockingTest {
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