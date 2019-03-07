/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.ui.doorhanger

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.drawable.Drawable
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.RadioButton
import android.widget.RadioGroup
import android.widget.TextView
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.PRIVATE
import mozilla.components.support.ktx.android.view.forEach

/**
 * Builder for creating a prompt [Doorhanger] providing a way to present decisions to users.
 *
 * @param title Title text for this prompt.
 * @param icon (Optional) icon to be displayed next to the title.
 * @param controlGroups A list of control groups to be displayed in the prompt.
 * @param buttons A list of buttons to be displayed in the prompt.
 * @param onDismiss that is called when the doorhanger is dismissed.
 */
class DoorhangerPrompt(
    private val title: String,
    private val icon: Drawable? = null,
    @VisibleForTesting(otherwise = PRIVATE) val controlGroups: List<ControlGroup> = listOf(),
    @VisibleForTesting(otherwise = PRIVATE) val buttons: List<Button> = listOf(),
    private val onDismiss: (() -> Unit)? = null
) {
    /**
     * Creates a [Doorhanger] from the [DoorhangerPrompt] configuration.
     */
    fun createDoorhanger(context: Context): Doorhanger {
        var doorhanger: Doorhanger? = null

        @SuppressLint("InflateParams")
        val view = LayoutInflater.from(context).inflate(
            R.layout.mozac_ui_doorhanger_prompt,
            null,
            false)

        view.findViewById<TextView>(R.id.title).text = title

        icon?.let { drawable ->
            view.findViewById<ImageView>(R.id.icon).setImageDrawable(drawable)
        }

        val buttonContainer = view.findViewById<LinearLayout>(R.id.buttons)
        buttons.forEach { button ->
            val buttonView = button.createView(context, buttonContainer) {
                doorhanger?.dismiss()
                button.onClick.invoke()
            }
            buttonContainer.addView(buttonView)
        }

        val controlsContainer = view.findViewById<LinearLayout>(R.id.controls)
        controlGroups.forEachIndexed { index, group ->
            val groupView = group.createView(context, controlsContainer)

            controlsContainer.addView(groupView)

            if (index < controlGroups.size - 1) {
                controlsContainer.addView(
                    createDividerView(
                        context,
                        controlsContainer
                    )
                )
            }
        }

        return Doorhanger(view, onDismiss).also {
            doorhanger = it
        }
    }

    /**
     *
     * @property label a string to display on the button
     * @property positive a boolean for whether the button is an affirmative button. The value is implicitly false if
     * omitted.
     */
    data class Button(
        val label: String,
        val positive: Boolean = false,
        val onClick: () -> Unit
    )

    /**
     * A group of controls to be displayed in a [DoorhangerPrompt].
     *
     * @param icon An (optional) icon to be shown next to the control group.
     * @param controls List of controls to be shown in this group.
     */
    data class ControlGroup(
        val icon: Drawable? = null,
        val controls: List<Control>
    )

    sealed class Control {
        internal abstract fun createView(context: Context, parent: ViewGroup): View

        /**
         * [Control] implementation for radio buttons. A radio button is a two-states button that can be either checked
         * or unchecked.
         */
        data class RadioButton(
            val label: String
        ) : Control() {
            internal val viewId = View.generateViewId()
            var checked = false

            override fun createView(context: Context, parent: ViewGroup): View {
                val view = LayoutInflater.from(context).inflate(
                    R.layout.mozac_ui_doorhanger_radiobutton,
                    parent,
                    false
                )

                return (view as android.widget.RadioButton).apply {
                    this.id = viewId
                    this.text = label
                }
            }
        }

        /**
         * [Control] implementation for checkboxes. A checkbox is a specific type of two-states button that can be
         * either checked or unchecked.
         */
        data class CheckBox(
            val label: String,
            var checked: Boolean
        ) : Control() {
            override fun createView(context: Context, parent: ViewGroup): View {
                val view = LayoutInflater.from(context).inflate(
                    R.layout.mozac_ui_doorhanger_checkbox,
                    parent,
                    false
                )

                return (view as android.widget.CheckBox).apply {
                    text = label
                    isChecked = checked
                    setOnCheckedChangeListener { _, isChecked -> checked = isChecked }
                }
            }
        }
    }
}

internal fun DoorhangerPrompt.ControlGroup.createView(context: Context, parent: ViewGroup): View {
    val view = LayoutInflater.from(context).inflate(
        R.layout.mozac_ui_doorhanger_controlgroup,
        parent,
        false)

    val iconView = view.findViewById<ImageView>(R.id.icon)
    if (icon == null) {
        iconView.visibility = View.INVISIBLE
    } else {
        iconView.setImageDrawable(icon)
    }

    val groupView = view.findViewById<RadioGroup>(R.id.group)

    controls.forEach { control ->
        val controlView = control.createView(context, groupView)
        groupView.addView(controlView)
    }

    groupView.setOnCheckedChangeListener { _, checkedId ->
        controls.forEach {
            if (it is DoorhangerPrompt.Control.RadioButton) {
                it.checked = it.viewId == checkedId
            }
        }
    }

    groupView.selectFirstRadioButton()

    return view
}

internal fun DoorhangerPrompt.Button.createView(context: Context, parent: ViewGroup, onClick: () -> Unit): View {
    val view = LayoutInflater.from(context).inflate(
        if (positive) {
            R.layout.mozac_ui_doorhanger_button_positive
        } else {
            R.layout.mozac_ui_doorhanger_button
        },
        parent,
        false
    ) as android.widget.Button

    view.text = label
    view.setOnClickListener { onClick.invoke() }

    return view
}

private fun createDividerView(context: Context, parent: ViewGroup): View {
    return LayoutInflater.from(context).inflate(
        R.layout.mozac_ui_doorhanger_divider,
        parent,
        false
    )
}

private fun ViewGroup.selectFirstRadioButton() {
    var found = false

    forEach { view ->
        if (view is RadioButton && !found) {
            found = true
            view.toggle()
        }
    }
}
