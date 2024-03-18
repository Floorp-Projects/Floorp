/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.customtabs.menu

import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import androidx.core.net.toUri
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.state.CustomTabConfig
import mozilla.components.browser.state.state.CustomTabMenuItem
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.concept.menu.candidate.MenuCandidate
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class CustomTabMenuCandidatesTest {

    @Test
    fun `return an empty list if there are no menu items`() {
        val customTabSessionState = createCustomTab(
            url = "https://mozilla.org",
            config = CustomTabConfig(menuItems = emptyList()),
        )

        assertEquals(
            emptyList<MenuCandidate>(),
            customTabSessionState.createCustomTabMenuCandidates(mock()),
        )
    }

    @Test
    fun `create a candidate for each menu item`() {
        val pendingIntent1 = mock<PendingIntent>()
        val pendingIntent2 = mock<PendingIntent>()
        val customTabSessionState = createCustomTab(
            url = "https://mozilla.org",
            config = CustomTabConfig(
                menuItems = listOf(
                    CustomTabMenuItem(
                        name = "item1",
                        pendingIntent = pendingIntent1,
                    ),
                    CustomTabMenuItem(
                        name = "item2",
                        pendingIntent = pendingIntent2,
                    ),
                ),
            ),
        )

        val context = mock<Context>()
        val intent = argumentCaptor<Intent>()
        val menuCandidates = customTabSessionState.createCustomTabMenuCandidates(context)

        assertEquals(2, menuCandidates.size)
        assertEquals("item1", menuCandidates[0].text)
        assertEquals("item2", menuCandidates[1].text)

        menuCandidates[0].onClick()
        verify(pendingIntent1).send(eq(context), anyInt(), intent.capture())
        assertEquals("https://mozilla.org".toUri(), intent.value.data)

        menuCandidates[1].onClick()
        verify(pendingIntent2).send(eq(context), anyInt(), intent.capture())
        assertEquals("https://mozilla.org".toUri(), intent.value.data)
    }
}
