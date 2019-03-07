/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts

import android.annotation.SuppressLint
import android.app.Dialog
import android.content.DialogInterface
import android.graphics.Color
import android.graphics.PorterDuff
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.annotation.VisibleForTesting
import androidx.appcompat.app.AlertDialog
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView

private const val KEY_SELECTED_COLOR = "KEY_SELECTED_COLOR"

/**
 * [android.support.v4.app.DialogFragment] implementation for a color picker dialog.
 */
internal class ColorPickerDialogFragment : PromptDialogFragment() {

    private lateinit var selectColorTexView: TextView

    @VisibleForTesting
    internal var selectedColor: Int
        get() = safeArguments.getInt(KEY_SELECTED_COLOR)
        set(value) {
            safeArguments.putInt(KEY_SELECTED_COLOR, value)
        }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val builder = AlertDialog.Builder(requireContext())
            .setCancelable(true)
            .setNegativeButton(R.string.mozac_feature_prompts_cancel) { _, _ ->
                feature?.onCancel(sessionId)
            }
            .setPositiveButton(R.string.mozac_feature_prompts_set_date) { _, _ ->
                onPositiveClickAction()
            }
            .setView(createDialogContentView())
        return builder.create()
    }

    override fun onCancel(dialog: DialogInterface?) {
        super.onCancel(dialog)
        feature?.onCancel(sessionId)
    }

    companion object {
        fun newInstance(sessionId: String, defaultColor: String): ColorPickerDialogFragment {
            val fragment = ColorPickerDialogFragment()
            val arguments = fragment.arguments ?: Bundle()

            with(arguments) {
                putString(KEY_SESSION_ID, sessionId)
                putInt(KEY_SELECTED_COLOR, defaultColor.toColor())
            }

            fragment.arguments = arguments

            return fragment
        }
    }

    @SuppressLint("InflateParams")
    internal fun createDialogContentView(): View {
        val inflater = LayoutInflater.from(requireContext())
        val view = inflater.inflate(R.layout.mozac_feature_prompts_color_picker_dialogs, null)

        initSelectedColor(view)
        initRecyclerView(view, inflater)

        return view
    }

    private fun onPositiveClickAction() {
        feature?.onConfirm(sessionId, selectedColor.toHexColor())
    }

    fun onColorChange(newColor: Int) {
        selectedColor = newColor
        selectColorTexView.changeColor(newColor)
    }

    /**
     * RecyclerView adapter for displaying color items.
     */
    internal class ColorAdapter(
        private val fragment: ColorPickerDialogFragment,
        private val inflater: LayoutInflater
    ) : RecyclerView.Adapter<RecyclerView.ViewHolder>() {

        @Suppress("MagicNumber")
        internal val defaultColors = arrayOf(
            Color.rgb(215, 57, 32),
            Color.rgb(255, 134, 5),
            Color.rgb(255, 203, 19),
            Color.rgb(95, 173, 71),
            Color.rgb(33, 161, 222),
            Color.rgb(16, 36, 87),
            Color.rgb(91, 32, 103),
            Color.rgb(212, 221, 228),
            Color.WHITE
        )

        override fun getItemCount(): Int = defaultColors.size

        override fun onCreateViewHolder(parent: ViewGroup, type: Int): RecyclerView.ViewHolder {
            val view = inflater.inflate(R.layout.mozac_feature_prompts_color_item, parent, false)
            return ColorViewHolder(view)
        }

        override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {

            with(holder as ColorViewHolder) {
                val color = defaultColors[position]
                bind(color, fragment)
            }
        }
    }

    /**
     * View holder for a color item.
     */
    internal class ColorViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
        fun bind(color: Int, fragment: ColorPickerDialogFragment) {

            with(itemView) {
                changeColor(color)
                tag = color

                setOnClickListener {
                    val newColor = it.tag as Int
                    fragment.onColorChange(newColor)
                }
            }
        }
    }

    private fun initSelectedColor(view: View) {
        selectColorTexView = view.findViewById(R.id.selected_color)
        selectColorTexView.changeColor(selectedColor)
    }

    private fun initRecyclerView(view: View, inflater: LayoutInflater) {
        view.findViewById<RecyclerView>(R.id.recyclerView).apply {
            layoutManager = LinearLayoutManager(context, RecyclerView.VERTICAL, false)
            adapter = ColorAdapter(this@ColorPickerDialogFragment, inflater)
        }
    }
}

private fun View.changeColor(newColor: Int) {
    background.setColorFilter(newColor, PorterDuff.Mode.MULTIPLY)
}

internal fun String.toColor(): Int {
    return try {
        Color.parseColor(this)
    } catch (e: IllegalArgumentException) {
        0
    }
}

@Suppress("MagicNumber")
internal fun Int.toHexColor(): String {
    return String.format("#%06x", 0xffffff and this)
}
