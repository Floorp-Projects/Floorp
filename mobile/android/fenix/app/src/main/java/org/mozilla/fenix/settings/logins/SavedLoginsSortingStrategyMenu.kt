/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins

import android.content.Context
import androidx.annotation.VisibleForTesting
import mozilla.components.browser.menu2.BrowserMenuController
import mozilla.components.concept.menu.candidate.HighPriorityHighlightEffect
import mozilla.components.concept.menu.candidate.TextMenuCandidate
import mozilla.components.concept.menu.candidate.TextStyle
import mozilla.components.support.ktx.android.content.getColorFromAttr
import org.mozilla.fenix.R
import org.mozilla.fenix.settings.logins.interactor.SavedLoginsInteractor

class SavedLoginsSortingStrategyMenu(
    private val context: Context,
    private val savedLoginsInteractor: SavedLoginsInteractor,
) {
    enum class Item(val strategyString: String) {
        AlphabeticallySort("ALPHABETICALLY"),
        LastUsedSort("LAST_USED"),
        ;

        companion object {
            fun fromString(strategyString: String) = when (strategyString) {
                AlphabeticallySort.strategyString -> AlphabeticallySort
                LastUsedSort.strategyString -> LastUsedSort
                else -> AlphabeticallySort
            }
        }
    }

    val menuController by lazy { BrowserMenuController() }

    @VisibleForTesting
    internal fun menuItems(itemToHighlight: Item): List<TextMenuCandidate> {
        val textStyle = TextStyle(
            color = context.getColorFromAttr(R.attr.textPrimary),
        )

        val highlight = HighPriorityHighlightEffect(
            backgroundTint = context.getColorFromAttr(R.attr.colorControlHighlight),
        )

        return listOf(
            TextMenuCandidate(
                text = context.getString(R.string.saved_logins_sort_strategy_alphabetically),
                textStyle = textStyle,
                effect = if (itemToHighlight == Item.AlphabeticallySort) highlight else null,
            ) {
                savedLoginsInteractor.onSortingStrategyChanged(
                    SortingStrategy.Alphabetically,
                )
            },
            TextMenuCandidate(
                text = context.getString(R.string.saved_logins_sort_strategy_last_used),
                textStyle = textStyle,
                effect = if (itemToHighlight == Item.LastUsedSort) highlight else null,
            ) {
                savedLoginsInteractor.onSortingStrategyChanged(
                    SortingStrategy.LastUsed,
                )
            },
        )
    }

    fun updateMenu(itemToHighlight: Item) {
        menuController.submitList(menuItems(itemToHighlight))
    }
}
