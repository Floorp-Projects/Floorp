/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2.view

import android.content.res.ColorStateList
import android.graphics.Color
import android.os.Build
import androidx.cardview.widget.CardView
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.menu2.R
import mozilla.components.concept.menu.MenuStyle
import mozilla.components.concept.menu.Side
import mozilla.components.concept.menu.candidate.DecorativeTextMenuCandidate
import mozilla.components.support.test.any
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.annotation.Config

@RunWith(AndroidJUnit4::class)
class MenuViewTest {

    private val items = listOf(
        DecorativeTextMenuCandidate("Hello"),
        DecorativeTextMenuCandidate("World")
    )
    private lateinit var menuView: MenuView
    private lateinit var cardView: CardView
    private lateinit var recyclerView: RecyclerView

    @Before
    fun setup() {
        menuView = spy(MenuView(testContext))
        cardView = menuView.findViewById(R.id.mozac_browser_menu_cardView)
        recyclerView = menuView.findViewById(R.id.mozac_browser_menu_recyclerView)
    }

    @Test
    fun `recyclerview adapter will have items for every menu item`() {
        assertNotNull(recyclerView)

        val recyclerAdapter = recyclerView.adapter!!
        assertNotNull(recyclerAdapter)
        assertEquals(0, recyclerAdapter.itemCount)

        menuView.submitList(items)
        assertEquals(2, recyclerAdapter.itemCount)
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.M])
    fun `setVisibleSide will be forwarded to scrollOnceToTheBottom on devices with Android M and below`() {
        doNothing().`when`(menuView).scrollOnceToTheBottom(any())

        menuView.setVisibleSide(Side.END)
        val layoutManager = recyclerView.layoutManager as LinearLayoutManager

        assertFalse(layoutManager.stackFromEnd)
        verify(menuView).scrollOnceToTheBottom(any())
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.N])
    fun `setVisibleSide changes stackFromEnd on devices with Android N and above`() {
        doNothing().`when`(menuView).scrollOnceToTheBottom(any())

        menuView.setVisibleSide(Side.END)
        val layoutManager = recyclerView.layoutManager as LinearLayoutManager

        assertTrue(layoutManager.stackFromEnd)
        verify(menuView, never()).scrollOnceToTheBottom(any())
    }

    @Test
    fun `setStyle changes background color`() {
        menuView.setStyle(MenuStyle(backgroundColor = Color.BLUE))
        assertEquals(ColorStateList.valueOf(Color.BLUE), cardView.cardBackgroundColor)
    }
}
