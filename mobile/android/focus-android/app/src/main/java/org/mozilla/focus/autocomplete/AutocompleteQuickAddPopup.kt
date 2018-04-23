package org.mozilla.focus.autocomplete

import android.content.Context
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.PopupWindow
import kotlinx.coroutines.experimental.launch
import mozilla.components.browser.domains.CustomDomains
import org.mozilla.focus.R
import kotlin.math.roundToInt

class AutocompleteQuickAddPopup(context: Context, url: String) : PopupWindow() {
    var onUrlAdded: (() -> Unit)? = null

    init {
        val view = LayoutInflater.from(context).inflate(R.layout.autocomplete_quick_add_popup, null)
        contentView = view

        setBackgroundDrawable(ColorDrawable(Color.TRANSPARENT))

        val button = view.findViewById<Button>(R.id.quick_add_autocomplete_button)
        button.setOnClickListener {
            val job = launch { CustomDomains.add(context, url) }
            job.invokeOnCompletion { onUrlAdded?.invoke() }
        }

        isFocusable = true

        height = ViewGroup.LayoutParams.WRAP_CONTENT
        width = ViewGroup.LayoutParams.WRAP_CONTENT

        elevation = context.resources.getDimension(R.dimen.menu_elevation)
    }

    fun show(anchor: View) {
        val y = (anchor.height * Y_POS_MULTIPLIER).roundToInt()
        super.showAtLocation(anchor, Gravity.CENTER_HORIZONTAL or Gravity.TOP, 0, y)
    }

    companion object {
        const val Y_POS_MULTIPLIER = 1.5
    }
}
