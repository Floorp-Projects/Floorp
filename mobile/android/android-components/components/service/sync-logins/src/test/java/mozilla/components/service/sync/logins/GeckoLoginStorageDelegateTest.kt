/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.sync.logins

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.test.TestCoroutineScope
import mozilla.components.concept.storage.Login
import mozilla.components.support.test.any
import mozilla.components.support.test.robolectric.testContext
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class GeckoLoginStorageDelegateTest {

    private val loginsStorage = SyncableLoginsStorage(testContext, "dummy-key")
    private lateinit var delegate: GeckoLoginStorageDelegate
    private lateinit var scope: TestCoroutineScope

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
        delegate = GeckoLoginStorageDelegate(lazy { loginsStorage }, scope)
    }

    @Test
    fun `WHEN onLoginsUsed is used THEN loginStorage should be touched`() {
        scope.launch {
            val login = createLogin("guid")

            delegate.onLoginUsed(login)
            verify(loginsStorage, times(1)).touch(any())
        }
    }
}

fun createLogin(
    guid: String = "id",
    password: String = "password",
    username: String = "username",
    origin: String = "https://www.origin.com",
    httpRealm: String = "httpRealm",
    formActionOrigin: String = "https://www.origin.com",
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
