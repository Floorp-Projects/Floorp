/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.awesomebar.layout

import android.content.res.ColorStateList
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageButton
import android.widget.ImageView
import android.widget.TextView
import androidx.core.widget.ImageViewCompat
import androidx.annotation.VisibleForTesting
import mozilla.components.browser.awesomebar.BrowserAwesomeBar
import mozilla.components.browser.awesomebar.R
import mozilla.components.browser.awesomebar.widget.FlowLayout
import mozilla.components.concept.awesomebar.AwesomeBar

// This is used for truncating title and description to prevent extreme cases
// from slowing down UI rendering e.g. in case of a bookmarklet or a data URI.
// So this is just to make the actual truncation in the view faster which
// is already using a truncated and "ellipsized" single line for suggestions.
// https://github.com/mozilla-mobile/android-components/issues/6985
@VisibleForTesting
@Suppress("MagicNumber", "TopLevelPropertyNaming")
internal var MAX_TEXT_LENGTH = 250

internal sealed class DefaultSuggestionViewHolder {
    /**
     * Default view holder for suggestions.
     */
    internal class Default(
        private val awesomeBar: BrowserAwesomeBar,
        view: View
    ) : SuggestionViewHolder(view) {
        private val titleView = view.findViewById<TextView>(R.id.mozac_browser_awesomebar_title).apply {
            setTextColor(awesomeBar.styling.titleTextColor)
        }
        private val descriptionView = view.findViewById<TextView>(R.id.mozac_browser_awesomebar_description).apply {
            setTextColor(awesomeBar.styling.descriptionTextColor)
        }
        private val iconView: ImageView = view.findViewById(R.id.mozac_browser_awesomebar_icon)
        private val iconIndicatorView: ImageView = view.findViewById(R.id.mozac_browser_awesomebar_icon_indicator)
        private val editView = view.findViewById<ImageButton>(R.id.mozac_browser_awesomebar_edit_suggestion).apply {
            ImageViewCompat.setImageTintList(this, ColorStateList.valueOf(awesomeBar.styling.descriptionTextColor))
        }

        override fun bind(suggestion: AwesomeBar.Suggestion, selectionListener: () -> Unit) {
            val title = if (suggestion.title.isNullOrEmpty()) suggestion.description else suggestion.title

            iconView.setImageBitmap(suggestion.icon)

            if (suggestion.indicatorIcon == null) {
                iconIndicatorView.visibility = View.GONE
            } else {
                iconIndicatorView.apply {
                    setImageDrawable(suggestion.indicatorIcon)
                    visibility = View.VISIBLE
                }
            }

            titleView.text = title?.take(MAX_TEXT_LENGTH)

            if (suggestion.description.isNullOrEmpty()) {
                descriptionView.visibility = View.GONE
            } else {
                descriptionView.visibility = View.VISIBLE
                descriptionView.text = suggestion.description?.take(MAX_TEXT_LENGTH)
            }

            view.setOnClickListener {
                suggestion.onSuggestionClicked?.invoke()
                selectionListener.invoke()
            }

            if (suggestion.editSuggestion.isNullOrEmpty()) {
                editView.visibility = View.GONE
            } else {
                editView.visibility = View.VISIBLE
                editView.setOnClickListener {
                    awesomeBar.editSuggestionListener?.invoke(suggestion.editSuggestion!!)
                }
            }
        }

        companion object {
            val LAYOUT_ID = R.layout.mozac_browser_awesomebar_item_generic
        }
    }

    /**
     * View holder for suggestions that contain chips.
     */
    internal class Chips(
        private val awesomeBar: BrowserAwesomeBar,
        view: View
    ) : SuggestionViewHolder(view) {
        private val chipsView = view.findViewById<FlowLayout>(R.id.mozac_browser_awesomebar_chips).apply {
            spacing = awesomeBar.styling.chipSpacing
        }
        private val inflater = LayoutInflater.from(view.context)
        private val iconView = view.findViewById<ImageView>(R.id.mozac_browser_awesomebar_icon)

        override fun bind(suggestion: AwesomeBar.Suggestion, selectionListener: () -> Unit) {
            chipsView.removeAllViews()

            iconView.setImageBitmap(suggestion.icon)

            suggestion
                .chips
                .forEach { chip ->
                    val view = inflater.inflate(
                        R.layout.mozac_browser_awesomebar_chip,
                        view as ViewGroup,
                        false
                    ) as TextView

                    view.setTextColor(awesomeBar.styling.chipTextColor)
                    view.setBackgroundColor(awesomeBar.styling.chipBackgroundColor)
                    view.text = chip.title.take(MAX_TEXT_LENGTH)
                    view.setOnClickListener {
                        suggestion.onChipClicked?.invoke(chip)
                        selectionListener.invoke()
                    }

                    chipsView.addView(view)
                }
        }

        companion object {
            val LAYOUT_ID = R.layout.mozac_browser_awesomebar_item_chips
        }
    }
}
