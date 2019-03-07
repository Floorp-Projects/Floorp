/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.awesomebar.layout

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageView
import android.widget.TextView
import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.components.browser.awesomebar.BrowserAwesomeBar
import mozilla.components.browser.awesomebar.R
import mozilla.components.browser.awesomebar.widget.FlowLayout
import mozilla.components.concept.awesomebar.AwesomeBar

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
        private val iconLoader = IconLoader(view.findViewById(R.id.mozac_browser_awesomebar_icon))

        override fun bind(suggestion: AwesomeBar.Suggestion, selectionListener: () -> Unit) {
            val title = if (suggestion.title.isNullOrEmpty()) suggestion.description else suggestion.title

            iconLoader.load(suggestion)

            titleView.text = title

            if (suggestion.description.isNullOrEmpty()) {
                descriptionView.visibility = View.GONE
            } else {
                descriptionView.visibility = View.VISIBLE
                descriptionView.text = suggestion.description
            }

            view.setOnClickListener {
                suggestion.onSuggestionClicked?.invoke()
                selectionListener.invoke()
            }
        }

        override fun recycle() {
            iconLoader.cancel()
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
        private val iconLoader = IconLoader(view.findViewById(R.id.mozac_browser_awesomebar_icon))

        override fun bind(suggestion: AwesomeBar.Suggestion, selectionListener: () -> Unit) {
            chipsView.removeAllViews()

            iconLoader.load(suggestion)

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
                    view.text = chip.title
                    view.setOnClickListener {
                        suggestion.onChipClicked?.invoke(chip)
                        selectionListener.invoke()
                    }

                    chipsView.addView(view)
                }
        }

        override fun recycle() {
            iconLoader.cancel()
        }

        companion object {
            val LAYOUT_ID = R.layout.mozac_browser_awesomebar_item_chips
        }
    }
}

/**
 * Helper class for loading icons asynchronously.
 */
internal class IconLoader(
    private val iconView: ImageView
) {
    private val scope = CoroutineScope(Dispatchers.Main)
    @VisibleForTesting
    internal var iconJob: Job? = null

    fun load(suggestion: AwesomeBar.Suggestion) {
        cancel()

        val icon = suggestion.icon ?: return

        iconJob = scope.launch {
            val bitmap = withContext(Dispatchers.IO) {
                icon.invoke(iconView.measuredWidth, iconView.measuredHeight)
            }

            if (bitmap != null) {
                iconView.setImageBitmap(bitmap)
            }
        }
    }

    fun cancel() {
        iconView.setImageBitmap(null)

        iconJob?.cancel()
    }
}
