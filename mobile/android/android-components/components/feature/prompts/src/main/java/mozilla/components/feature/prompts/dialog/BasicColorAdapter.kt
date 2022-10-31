/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.dialog

import android.graphics.Color
import android.graphics.Rect
import android.graphics.drawable.Drawable
import android.util.TypedValue
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.annotation.ColorInt
import androidx.annotation.VisibleForTesting
import androidx.core.content.ContextCompat
import androidx.core.graphics.BlendModeColorFilterCompat.createBlendModeColorFilterCompat
import androidx.core.graphics.BlendModeCompat
import androidx.core.graphics.BlendModeCompat.SRC_IN
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.feature.prompts.R
import mozilla.components.support.utils.ColorUtils

/**
 * Represents an item in the [BasicColorAdapter] list.
 *
 * @property color color int that this item corresponds to.
 * @property contentDescription accessibility description of this color.
 * @property selected if true, this is the color that will be set when the dialog is closed.
 */
data class ColorItem(
    @ColorInt val color: Int,
    val contentDescription: String,
    val selected: Boolean = false,
)

private object ColorItemDiffCallback : DiffUtil.ItemCallback<ColorItem>() {
    override fun areItemsTheSame(oldItem: ColorItem, newItem: ColorItem) =
        oldItem.color == newItem.color

    override fun areContentsTheSame(oldItem: ColorItem, newItem: ColorItem) =
        oldItem == newItem
}

/**
 * RecyclerView adapter for displaying color items.
 */
internal class BasicColorAdapter(
    private val onColorSelected: (Int) -> Unit,
) : ListAdapter<ColorItem, ColorViewHolder>(ColorItemDiffCallback) {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ColorViewHolder {
        val view = LayoutInflater
            .from(parent.context)
            .inflate(R.layout.mozac_feature_prompts_color_item, parent, false)
        return ColorViewHolder(view, onColorSelected)
    }

    override fun onBindViewHolder(holder: ColorViewHolder, position: Int) {
        holder.bind(getItem(position))
    }
}

/**
 * View holder for a color item.
 */
internal class ColorViewHolder(
    itemView: View,
    private val onColorSelected: (Int) -> Unit,
) : RecyclerView.ViewHolder(itemView), View.OnClickListener {
    @VisibleForTesting
    @ColorInt
    internal var color: Int = Color.BLACK

    private val checkDrawable: Drawable? by lazy {
        // Get the height of the row
        val typedValue = TypedValue()
        itemView.context.theme.resolveAttribute(
            android.R.attr.listPreferredItemHeight,
            typedValue,
            true,
        )
        var height = typedValue.getDimension(itemView.context.resources.displayMetrics).toInt()

        // Remove padding for the shadow
        val backgroundPadding = Rect()
        ContextCompat.getDrawable(itemView.context, R.drawable.color_picker_row_bg)?.getPadding(backgroundPadding)
        height -= backgroundPadding.top + backgroundPadding.bottom

        ContextCompat.getDrawable(itemView.context, R.drawable.color_picker_checkmark)?.apply {
            setBounds(0, 0, height, height)
        }
    }

    init {
        itemView.setOnClickListener(this)
    }

    fun bind(colorItem: ColorItem) {
        // Save the color for the onClick callback
        color = colorItem.color

        // Set the background to look like this item's color
        itemView.background = itemView.background.apply {
            colorFilter = createBlendModeColorFilterCompat(
                colorItem.color,
                BlendModeCompat.MODULATE,
            )
        }
        itemView.contentDescription = colorItem.contentDescription

        // Display the check mark
        val check = if (colorItem.selected) {
            checkDrawable?.apply {
                val readableColor = ColorUtils.getReadableTextColor(color)
                colorFilter = createBlendModeColorFilterCompat(readableColor, SRC_IN)
            }
        } else {
            null
        }
        itemView.isActivated = colorItem.selected
        (itemView as TextView).setCompoundDrawablesRelative(check, null, null, null)
    }

    override fun onClick(v: View?) {
        onColorSelected(color)
    }
}
