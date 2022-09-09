/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins

import android.content.Context
import androidx.appcompat.view.ContextThemeWrapper
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.concept.menu.candidate.HighPriorityHighlightEffect
import mozilla.components.support.ktx.android.content.getColorFromAttr
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.settings.logins.SavedLoginsSortingStrategyMenu.Item
import org.mozilla.fenix.settings.logins.interactor.SavedLoginsInteractor

@RunWith(FenixRobolectricTestRunner::class)
class SavedLoginsSortingStrategyMenuTest {

    private lateinit var context: Context
    private lateinit var interactor: SavedLoginsInteractor
    private lateinit var menu: SavedLoginsSortingStrategyMenu

    @Before
    fun setup() {
        context = ContextThemeWrapper(testContext, R.style.NormalTheme)
        interactor = mockk()
        menu = SavedLoginsSortingStrategyMenu(context, interactor)
    }

    @Test
    fun `item enum can be deserialized from string`() {
        assertEquals(Item.AlphabeticallySort, Item.fromString("ALPHABETICALLY"))
        assertEquals(Item.LastUsedSort, Item.fromString("LAST_USED"))
        assertEquals(Item.AlphabeticallySort, Item.fromString("OTHER"))
    }

    @Test
    fun `effect is set on alphabetical sort candidate`() {
        val (name, lastUsed) = menu.menuItems(Item.AlphabeticallySort)
        assertEquals(
            HighPriorityHighlightEffect(context.getColorFromAttr(R.attr.colorControlHighlight)),
            name.effect,
        )
        assertNull(lastUsed.effect)
    }

    @Test
    fun `effect is set on last used sort candidate`() {
        val (name, lastUsed) = menu.menuItems(Item.LastUsedSort)
        assertNull(name.effect)
        assertEquals(
            HighPriorityHighlightEffect(context.getColorFromAttr(R.attr.colorControlHighlight)),
            lastUsed.effect,
        )
    }

    @Test
    fun `candidates call interactor on click`() {
        val (name, lastUsed) = menu.menuItems(Item.AlphabeticallySort)
        every { interactor.onSortingStrategyChanged(any()) } just Runs

        name.onClick()
        verify {
            interactor.onSortingStrategyChanged(SortingStrategy.Alphabetically)
        }

        lastUsed.onClick()
        verify {
            interactor.onSortingStrategyChanged(
                SortingStrategy.LastUsed,
            )
        }
    }
}
