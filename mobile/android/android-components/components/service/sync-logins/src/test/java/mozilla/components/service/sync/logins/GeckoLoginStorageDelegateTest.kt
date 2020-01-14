package mozilla.components.service.sync.logins

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.launch
import kotlinx.coroutines.test.TestCoroutineScope
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Before
import mozilla.components.concept.storage.Login
import mozilla.components.lib.dataprotect.SecureAbove22Preferences
import mozilla.components.support.test.any
import mozilla.components.support.test.robolectric.testContext
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@ExperimentalCoroutinesApi
@RunWith(RobolectricTestRunner::class)
class GeckoLoginStorageDelegateTest {

    private lateinit var loginsStorage: AsyncLoginsStorage
    private lateinit var keystore: SecureAbove22Preferences
    private lateinit var delegate: GeckoLoginStorageDelegate
    private lateinit var scope: TestCoroutineScope

    @Before
    @Config(sdk = [21])
    fun before() {
        loginsStorage = mockLoginsStorage()
        keystore = SecureAbove22Preferences(testContext, "name")
        scope = TestCoroutineScope()
        delegate = GeckoLoginStorageDelegate(loginsStorage, keystore, { false }, scope)
    }

    @Test
    @Config(sdk = [21])
    fun `WHEN passed false for shouldAutofill onLoginsFetch returns early`() {
        scope.launch {
            delegate.onLoginFetch("login")
            verify(loginsStorage, times(0)).touch(any()).await()
        }
    }

    @Test
    @Config(sdk = [21])
    fun `WHEN passed true for shouldAutofill onLoginsFetch does not return early`() {
        delegate = GeckoLoginStorageDelegate(loginsStorage, keystore, { true }, scope)

        scope.launch {
            delegate.onLoginFetch("login")
            verify(loginsStorage, times(1)).touch(any()).await()
        }
    }

    @Test
    @Config(sdk = [21])
    fun `WHEN onLoginsUsed is used THEN loginStorage should be touched`() {
        scope.launch {
            val login = createLogin("guid")

            delegate.onLoginUsed(login)
            verify(loginsStorage, times(1)).touch(any()).await()
        }
    }

    @Test
    @Config(sdk = [21])
    fun `WHEN guid is null or empty THEN should create a new record`() {
        val serverPassword = createServerPassword()

        val fromNull = delegate.getPersistenceOperation(createLogin(guid = null), serverPassword)
        val fromEmpty = delegate.getPersistenceOperation(createLogin(guid = ""), serverPassword)

        assertEquals(Operation.CREATE, fromNull)
        assertEquals(Operation.CREATE, fromEmpty)
    }

    @Test
    @Config(sdk = [21])
    fun `WHEN guid matches existing record AND saved record has an empty username THEN should update existing record`() {
        val serverPassword = createServerPassword(id = "1", username = "")
        val login = createLogin(guid = "1")

        assertEquals(Operation.UPDATE, delegate.getPersistenceOperation(login, serverPassword))
    }

    @Test
    @Config(sdk = [21])
    fun `WHEN guid matches existing record AND new username is different from saved THEN should create new record`() {
        val serverPassword = createServerPassword(id = "1", username = "old")
        val login = createLogin(guid = "1", username = "new")

        assertEquals(Operation.CREATE, delegate.getPersistenceOperation(login, serverPassword))
    }

    @Test
    @Config(sdk = [21])
    fun `WHEN guid and username match THEN update existing record`() {
        val serverPassword = createServerPassword(id = "1", username = "username")
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
        val serverPassword = createServerPassword(
            id = "spId",
            hostname = "spHost",
            username = "spUser",
            password = "spPassword",
            httpRealm = "spHttpRealm",
            formSubmitUrl = "spFormSubmitUrl"
        )

        val expected = createServerPassword(
            id = "spId",
            hostname = "origin",
            username = "username",
            password = "password",
            httpRealm = "httpRealm",
            formSubmitUrl = "fao"
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
        val serverPassword = createServerPassword(
            id = "spId",
            hostname = "spHost",
            username = "spUser",
            password = "spPassword",
            httpRealm = "spHttpRealm",
            formSubmitUrl = "spFormSubmitUrl"
        )

        val expected = createServerPassword(
            id = "spId",
            hostname = "origin",
            username = "username",
            password = "password",
            httpRealm = "spHttpRealm",
            formSubmitUrl = "spFormSubmitUrl"
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
        val serverPassword = createServerPassword(
            id = "spId",
            hostname = "spHost",
            username = "spUser",
            password = "spPassword",
            httpRealm = "spHttpRealm",
            formSubmitUrl = "spFormSubmitUrl"
        )

        val expected = createServerPassword(
            id = "spId",
            hostname = "spHost",
            username = "spUser",
            password = "spPassword",
            httpRealm = "spHttpRealm",
            formSubmitUrl = "spFormSubmitUrl"
        )

        assertEquals(expected, serverPassword.mergeWithLogin(login))
    }
}

fun mockLoginsStorage(): AsyncLoginsStorage {
    val loginsStorage = mock<AsyncLoginsStorage>()
    var isLocked = true

    fun <T> setLockedWhen(on: T, newIsLocked: Boolean) {
        `when`(on).thenAnswer {
            isLocked = newIsLocked
            Unit
        }
    }

    setLockedWhen(loginsStorage.ensureLocked(), true)
    setLockedWhen(loginsStorage.lock(), true)

    setLockedWhen(loginsStorage.ensureUnlocked(any<ByteArray>()), false)
    setLockedWhen(loginsStorage.ensureUnlocked(any<String>()), false)
    setLockedWhen(loginsStorage.unlock(any<String>()), false)
    setLockedWhen(loginsStorage.unlock(any<ByteArray>()), false)

    `when`(loginsStorage.isLocked()).thenAnswer { isLocked }
    `when`(loginsStorage.touch(any())).thenAnswer { }

    return loginsStorage
}

fun createLogin(guid: String?, username: String = "username") = Login(
    guid = guid,
    username = username,
    password = "password",
    origin = "origin"
)

fun createServerPassword(
    id: String = "id",
    password: String = "password",
    username: String = "username",
    hostname: String = "hostname",
    httpRealm: String = "httpRealm",
    formSubmitUrl: String = "formsubmiturl",
    usernameField: String = "usernameField",
    passwordField: String = "passwordField"

) = ServerPassword(
    id = id,
    hostname = hostname,
    password = password,
    username = username,
    httpRealm = httpRealm,
    formSubmitURL = formSubmitUrl,
    usernameField = usernameField,
    passwordField = passwordField
)
