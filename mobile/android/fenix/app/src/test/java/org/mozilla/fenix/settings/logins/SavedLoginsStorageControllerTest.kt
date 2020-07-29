/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins

import androidx.navigation.NavController
import androidx.navigation.NavDestination
import io.mockk.Runs
import io.mockk.coEvery
import io.mockk.coVerify
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.TestCoroutineScope
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.concept.storage.Login
import mozilla.components.service.sync.logins.SyncableLoginsStorage
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.directionsEq
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.settings.logins.controller.SavedLoginsStorageController
import org.mozilla.fenix.settings.logins.fragment.EditLoginFragmentDirections

@ExperimentalCoroutinesApi
@RunWith(FenixRobolectricTestRunner::class)
class SavedLoginsStorageControllerTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule(TestCoroutineDispatcher())

    private val passwordsStorage: SyncableLoginsStorage = mockk(relaxed = true)
    private lateinit var controller: SavedLoginsStorageController
    private val navController: NavController = mockk(relaxed = true)
    private val loginsFragmentStore: LoginsFragmentStore = mockk(relaxed = true)
    private val scope = TestCoroutineScope()
    private val ioDispatcher = TestCoroutineDispatcher()
    private val loginMock: Login = mockk(relaxed = true)

    @Before
    fun setup() {
        every { navController.currentDestination } returns NavDestination("").apply {
            id = R.id.loginDetailFragment
        }
        coEvery { passwordsStorage.get(any()) } returns loginMock
        every { loginsFragmentStore.dispatch(any()) } returns mockk()

        controller = SavedLoginsStorageController(
            passwordsStorage = passwordsStorage,
            viewLifecycleScope = scope,
            navController = navController,
            loginsFragmentStore = loginsFragmentStore,
            ioDispatcher = ioDispatcher
        )
    }

    @After
    fun cleanUp() {
        scope.cleanupTestCoroutines()
        ioDispatcher.cleanupTestCoroutines()
    }

    @Test
    fun `WHEN a login is deleted, THEN navigate back to the previous page`() = scope.runBlockingTest {
        val loginId = "id"
        coEvery { passwordsStorage.delete(any()) } returns true
        controller.delete(loginId)

        coVerify {
            passwordsStorage.delete(loginId)
            navController.popBackStack(R.id.savedLoginsFragment, false)
        }
    }

    @Test
    fun `WHEN fetching the login list, THEN update the state in the store`() = scope.runBlockingTest {
        val login = Login(
            guid = "id",
            origin = "https://www.test.co.gov.org",
            username = "user123",
            password = "securePassword1",
            httpRealm = "httpRealm",
            formActionOrigin = ""
        )
        coEvery { passwordsStorage.list() } returns listOf(login)

        controller.fetchLoginDetails(login.guid!!)

        val expectedLogin = login.mapToSavedLogin()

        coVerify {
            passwordsStorage.list()
            loginsFragmentStore.dispatch(
                LoginsAction.UpdateCurrentLogin(
                    expectedLogin
                )
            )
        }
    }

    @Test
    fun `WHEN saving an update to an item, THEN navigate to login detail view`() = scope.runBlockingTest {
        val oldLogin = Login(
            guid = "id",
            origin = "https://www.test.co.gov.org",
            username = "user123",
            password = "securePassword1",
            httpRealm = "httpRealm",
            formActionOrigin = ""
        )

        coEvery { passwordsStorage.get(any()) } returns oldLogin
        coEvery { passwordsStorage.update(any()) } just Runs

        controller.save(oldLogin.guid!!, "newUsername", "newPassword")

        val directions =
            EditLoginFragmentDirections.actionEditLoginFragmentToLoginDetailFragment(
                oldLogin.guid!!
            )

        val newLogin = Login(
            guid = "id",
            origin = "https://www.test.co.gov.org",
            username = "newUsername",
            password = "newPassword",
            httpRealm = "httpRealm",
            formActionOrigin = ""
        )

        val expectedNewList = listOf(newLogin.mapToSavedLogin())

        coVerify {
            passwordsStorage.get(oldLogin.guid!!)
            passwordsStorage.update(newLogin)
            loginsFragmentStore.dispatch(
                LoginsAction.UpdateLoginsList(
                    expectedNewList
                )
            )
            navController.navigate(directionsEq(directions))
        }
    }

    @Test
    fun `WHEN finding login dupes, THEN update duplicates in the store`() = scope.runBlockingTest {
        val login = Login(
            guid = "id",
            origin = "https://www.test.co.gov.org",
            username = "user123",
            password = "securePassword1",
            httpRealm = "httpRealm",
            formActionOrigin = ""
        )

        val login2 = Login(
            guid = "id2",
            origin = "https://www.test.co.gov.org",
            username = "user1234",
            password = "securePassword1",
            httpRealm = "httpRealm",
            formActionOrigin = ""
        )

        coEvery { passwordsStorage.get(any()) } returns login

        val dupeList = listOf(login2)

        coEvery {
            passwordsStorage.getPotentialDupesIgnoringUsername(any())
        } returns dupeList

        controller.findPotentialDuplicates(login.guid!!)

        val expectedDupeList = dupeList.map { it.mapToSavedLogin() }

        coVerify {
            passwordsStorage.getPotentialDupesIgnoringUsername(login)
            loginsFragmentStore.dispatch(
                LoginsAction.ListOfDupes(
                    dupeList = expectedDupeList
                )
            )
        }
    }
}
