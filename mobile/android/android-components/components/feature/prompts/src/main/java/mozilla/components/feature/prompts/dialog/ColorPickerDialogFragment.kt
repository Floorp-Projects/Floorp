/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.dialog

import android.annotation.SuppressLint
import android.app.Dialog
import android.content.DialogInterface
import android.graphics.Color
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import androidx.annotation.ColorInt
import androidx.annotation.VisibleForTesting
import androidx.appcompat.app.AlertDialog
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.feature.prompts.R

private const val KEY_SELECTED_COLOR = "KEY_SELECTED_COLOR"

private const val RGB_BIT_MASK = 0xffffff

/**
 * [androidx.fragment.app.DialogFragment] implementation for a color picker dialog.
 */
internal class ColorPickerDialogFragment : PromptDialogFragment(), DialogInterface.OnClickListener {

    @ColorInt
    private var initiallySelectedCustomColor: Int? = null
    private lateinit var defaultColors: List<ColorItem>
    private lateinit var listAdapter: BasicColorAdapter

    @VisibleForTesting
    internal var selectedColor: Int
        get() = safeArguments.getInt(KEY_SELECTED_COLOR)
        set(value) {
            safeArguments.putInt(KEY_SELECTED_COLOR, value)
        }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog =
        AlertDialog.Builder(requireContext())
            .setCancelable(true)
            .setTitle(R.string.mozac_feature_prompts_choose_a_color)
            .setNegativeButton(R.string.mozac_feature_prompts_cancel, this)
            .setPositiveButton(R.string.mozac_feature_prompts_set_date, this)
            .setView(createDialogContentView())
            .create()

    override fun onCancel(dialog: DialogInterface) {
        super.onCancel(dialog)
        onClick(dialog, DialogInterface.BUTTON_NEGATIVE)
    }

    override fun onClick(dialog: DialogInterface?, which: Int) {
        when (which) {
            DialogInterface.BUTTON_POSITIVE ->
                feature?.onConfirm(sessionId, promptRequestUID, selectedColor.toHexColor())
            DialogInterface.BUTTON_NEGATIVE -> feature?.onCancel(sessionId, promptRequestUID)
        }
    }

    @SuppressLint("InflateParams")
    internal fun createDialogContentView(): View {
        val view = LayoutInflater
            .from(requireContext())
            .inflate(R.layout.mozac_feature_prompts_color_picker_dialogs, null)

        // Save the color selected when this dialog opened to show at the end
        initiallySelectedCustomColor = selectedColor

        // Load list of colors from resources
        val typedArray = resources.obtainTypedArray(R.array.mozac_feature_prompts_default_colors)

        defaultColors = List(typedArray.length()) { i ->
            val color = typedArray.getColor(i, Color.BLACK)
            if (color == initiallySelectedCustomColor) {
                // No need to save the initial color, its already in the list
                initiallySelectedCustomColor = null
            }

            color.toColorItem()
        }
        typedArray.recycle()

        setupRecyclerView(view)
        onColorChange(selectedColor)
        return view
    }

    private fun setupRecyclerView(view: View) {
        listAdapter = BasicColorAdapter(this::onColorChange)
        view.findViewById<RecyclerView>(R.id.recyclerView).apply {
            layoutManager = LinearLayoutManager(context, RecyclerView.VERTICAL, false).apply {
                stackFromEnd = true
            }
            adapter = listAdapter
            setHasFixedSize(true)
            itemAnimator = null
        }
    }

    /**
     * Called when a new color is selected by the user.
     */
    @VisibleForTesting
    internal fun onColorChange(newColor: Int) {
        selectedColor = newColor

        val colorItems = defaultColors.toMutableList()
        val index = colorItems.indexOfFirst { it.color == newColor }
        val lastColor = if (index > -1) {
            colorItems[index] = colorItems[index].copy(selected = true)
            initiallySelectedCustomColor
        } else {
            newColor
        }
        if (lastColor != null) {
            colorItems.add(lastColor.toColorItem(selected = lastColor == newColor))
        }

        listAdapter.submitList(colorItems)
    }

    companion object {

        fun newInstance(
            sessionId: String,
            promptRequestUID: String,
            shouldDismissOnLoad: Boolean,
            defaultColor: String,
        ) = ColorPickerDialogFragment().apply {
            arguments = (arguments ?: Bundle()).apply {
                putString(KEY_SESSION_ID, sessionId)
                putString(KEY_PROMPT_UID, promptRequestUID)
                putBoolean(KEY_SHOULD_DISMISS_ON_LOAD, shouldDismissOnLoad)
                putInt(KEY_SELECTED_COLOR, defaultColor.toColor())
            }
        }
    }
}

internal fun Int.toColorItem(selected: Boolean = false): ColorItem {
    return ColorItem(
        color = this,
        contentDescription = toHexColor(),
        selected = selected,
    )
}

internal fun String.toColor(): Int {
    return try {
        Color.parseColor(this)
    } catch (e: IllegalArgumentException) {
        Color.BLACK
    }
}

internal fun Int.toHexColor(): String {
    return String.format("#%06x", RGB_BIT_MASK and this)
}
