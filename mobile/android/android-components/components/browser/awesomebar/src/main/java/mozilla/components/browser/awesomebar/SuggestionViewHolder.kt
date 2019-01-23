/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.awesomebar

import android.support.v7.widget.RecyclerView
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageView
import android.widget.TextView
import mozilla.components.browser.awesomebar.layout.FlowLayout
import mozilla.components.concept.awesomebar.AwesomeBar

/**
 * [RecyclerView.ViewHolder] implementations.
 */
internal sealed class SuggestionViewHolder(
    itemView: View
) : RecyclerView.ViewHolder(itemView) {
    /**
     * Binds the views in the holder to the passed suggestion.
     */
    abstract fun bind(suggestion: AwesomeBar.Suggestion)

    /**
     * Default view holder for suggestions.
     */
    internal class DefaultSuggestionViewHolder(
        private val awesomeBar: BrowserAwesomeBar,
        itemView: View
    ) : SuggestionViewHolder(itemView) {
        private val titleView = itemView.findViewById<TextView>(R.id.mozac_browser_awesomebar_title).apply {
            setTextColor(awesomeBar.styling.titleTextColor)
        }
        private val descriptionView = itemView.findViewById<TextView>(R.id.mozac_browser_awesomebar_description).apply {
            setTextColor(awesomeBar.styling.descriptionTextColor)
        }
        private val iconView = itemView.findViewById<ImageView>(R.id.mozac_browser_awesomebar_icon)

        override fun bind(suggestion: AwesomeBar.Suggestion) {
            val title = if (suggestion.title.isNullOrEmpty()) suggestion.description else suggestion.title

            val icon = suggestion.icon.invoke(iconView.measuredWidth, iconView.measuredHeight)
            iconView.setImageBitmap(icon)

            titleView.text = title
            descriptionView.text = suggestion.description

            itemView.setOnClickListener {
                suggestion.onSuggestionClicked?.invoke()
                awesomeBar.listener?.invoke()
            }
        }

        companion object {
            val LAYOUT_ID = R.layout.mozac_browser_awesomebar_item_generic
        }
    }

    /**
     * View holder for suggestions that contain chips.
     */
    internal class ChipsSuggestionViewHolder(
        private val awesomeBar: BrowserAwesomeBar,
        itemView: View
    ) : SuggestionViewHolder(itemView) {
        private val iconView = itemView.findViewById<ImageView>(R.id.mozac_browser_awesomebar_icon)
        private val chipsView = itemView.findViewById<FlowLayout>(R.id.mozac_browser_awesomebar_chips).apply {
            spacing = awesomeBar.styling.chipSpacing
        }

        override fun bind(suggestion: AwesomeBar.Suggestion) {
            chipsView.removeAllViews()

            val inflater = LayoutInflater.from(itemView.context)

            suggestion.icon.invoke(iconView.measuredWidth, iconView.measuredHeight)?.let { bitmap ->
                iconView.setImageBitmap(bitmap)
            }

            suggestion
                .chips
                .forEach { chip ->
                    val view = inflater.inflate(
                        R.layout.mozac_browser_awesomebar_chip,
                        itemView as ViewGroup,
                        false
                    ) as TextView

                    view.setTextColor(awesomeBar.styling.chipTextColor)
                    view.setBackgroundColor(awesomeBar.styling.chipBackgroundColor)
                    view.text = chip.title
                    view.setOnClickListener {
                        suggestion.onChipClicked?.invoke(chip)
                        awesomeBar.listener?.invoke()
                    }

                    chipsView.addView(view)
                }
        }

        companion object {
            val LAYOUT_ID = R.layout.mozac_browser_awesomebar_item_chips
        }
    }
}
