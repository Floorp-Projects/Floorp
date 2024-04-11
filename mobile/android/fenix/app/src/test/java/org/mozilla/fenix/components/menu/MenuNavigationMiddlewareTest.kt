/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu

import androidx.navigation.NavController
import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.test.runTest
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.components.menu.middleware.MenuNavigationMiddleware
import org.mozilla.fenix.components.menu.store.MenuAction
import org.mozilla.fenix.components.menu.store.MenuState
import org.mozilla.fenix.components.menu.store.MenuStore
import org.mozilla.fenix.ext.nav

class MenuNavigationMiddlewareTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val scope = coroutinesTestRule.scope

    private val navController: NavController = mockk(relaxed = true)

    private lateinit var store: MenuStore
    private lateinit var middleware: MenuNavigationMiddleware

    @Before
    fun setup() {
        middleware = MenuNavigationMiddleware(
            navController = navController,
            scope = scope,
        )
        store = MenuStore(
            initialState = MenuState(),
            middleware = listOf(
                middleware,
            ),
        )
    }

    @Test
    fun `WHEN navigate to settings action is dispatched THEN navigate to settings`() = runTest {
        store.dispatch(MenuAction.Navigate.Settings).join()

        verify {
            navController.nav(
                R.id.menuDialogFragment,
                MenuDialogFragmentDirections.actionGlobalSettingsFragment(),
            )
        }
    }
}
