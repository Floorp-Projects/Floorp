/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2.view

import android.content.Context
import android.os.Build
import android.os.Build.VERSION.SDK_INT
import android.util.AttributeSet
import android.view.LayoutInflater
import android.view.View
import android.widget.FrameLayout
import androidx.annotation.VisibleForTesting
import androidx.cardview.widget.CardView
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.browser.menu2.R
import mozilla.components.browser.menu2.adapter.MenuCandidateListAdapter
import mozilla.components.concept.menu.MenuStyle
import mozilla.components.concept.menu.Side
import mozilla.components.concept.menu.candidate.MenuCandidate
import mozilla.components.concept.menu.candidate.NestedMenuCandidate
import mozilla.components.support.ktx.android.view.onNextGlobalLayout

/**
 * A popup menu composed of [MenuCandidate] objects.
 */
class MenuView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0,
) : FrameLayout(context, attrs, defStyleAttr) {

    private val layoutManager = LinearLayoutManager(context, RecyclerView.VERTICAL, false)
    private val menuAdapter = MenuCandidateListAdapter(
        inflater = LayoutInflater.from(context),
        dismiss = { onDismiss() },
        reopenMenu = { onReopenMenu(it) },
    )
    private val cardView: CardView
    private val recyclerView: RecyclerView

    /**
     * Called when the menu is clicked and should be dismissed.
     */
    var onDismiss: () -> Unit = {}

    /**
     * Called when a nested menu should be opened.
     */
    var onReopenMenu: (NestedMenuCandidate?) -> Unit = {}

    init {
        View.inflate(context, R.layout.mozac_browser_menu2_view, this)

        cardView = findViewById(R.id.mozac_browser_menu_cardView)
        recyclerView = findViewById(R.id.mozac_browser_menu_recyclerView)
        recyclerView.layoutManager = layoutManager
        recyclerView.adapter = menuAdapter
    }

    /**
     * Changes the contents of the menu.
     */
    fun submitList(list: List<MenuCandidate>?) = menuAdapter.submitList(list)

    /**
     * Displays either the start or the end of the list.
     */
    fun setVisibleSide(side: Side) {
        if (SDK_INT >= Build.VERSION_CODES.N) {
            layoutManager.stackFromEnd = side == Side.END
        } else {
            // In devices with Android 6 and below stackFromEnd is not working properly,
            // as a result, we have to provided a backwards support.
            // See: https://github.com/mozilla-mobile/android-components/issues/3211
            if (side == Side.END) scrollOnceToTheBottom(recyclerView)
        }
    }

    /**
     * Sets the background color for the menu view.
     */
    fun setStyle(style: MenuStyle) {
        style.backgroundColor?.let { cardView.setCardBackgroundColor(it) }
    }

    @VisibleForTesting
    internal fun scrollOnceToTheBottom(recyclerView: RecyclerView) {
        recyclerView.onNextGlobalLayout {
            recyclerView.adapter?.let { recyclerView.scrollToPosition(it.itemCount - 1) }
        }
    }
}
