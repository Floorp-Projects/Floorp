/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.sitepermissions

import android.annotation.SuppressLint
import android.app.Dialog
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.View.VISIBLE
import android.view.Window
import android.widget.CheckBox
import android.widget.ImageView
import android.widget.TextView
import android.view.ViewGroup
import android.widget.Button
import android.widget.LinearLayout.LayoutParams
import androidx.appcompat.app.AppCompatDialogFragment
import androidx.core.content.ContextCompat

internal const val KEY_SESSION_ID = "KEY_SESSION_ID"
internal const val KEY_TITLE = "KEY_TITLE"
private const val KEY_USER_CHECK_BOX = "KEY_USER_CHECK_BOX"
private const val KEY_DIALOG_GRAVITY = "KEY_DIALOG_GRAVITY"
private const val KEY_DIALOG_WIDTH_MATCH_PARENT = "KEY_DIALOG_WIDTH_MATCH_PARENT"
private const val KEY_TITLE_ICON = "KEY_TITLE_ICON"
private const val KEY_POSITIVE_BUTTON_BACKGROUND_COLOR = "KEY_POSITIVE_BUTTON_BACKGROUND_COLOR"
private const val KEY_POSITIVE_BUTTON_TEXT_COLOR = "KEY_POSITIVE_BUTTON_TEXT_COLOR"
private const val KEY_SHOULD_INCLUDE_CHECKBOX = "KEY_SHOULD_INCLUDE_CHECKBOX"
private const val KEY_IS_NOTIFICATION_REQUEST = "KEY_IS_NOTIFICATION_REQUEST"
private const val DEFAULT_VALUE = Int.MAX_VALUE

internal class SitePermissionsDialogFragment : AppCompatDialogFragment() {

    internal var feature: SitePermissionsFeature? = null

    internal val sessionId: String by lazy { safeArguments.getString(KEY_SESSION_ID) }

    internal val title: String by lazy { safeArguments.getString(KEY_TITLE) }

    internal val dialogGravity: Int by lazy { safeArguments.getInt(KEY_DIALOG_GRAVITY, DEFAULT_VALUE) }

    internal val dialogShouldWidthMatchParent: Boolean by lazy {
        safeArguments.getBoolean(KEY_DIALOG_WIDTH_MATCH_PARENT)
    }

    internal val shouldIncludeCheckBox: Boolean by lazy { safeArguments.getBoolean(KEY_SHOULD_INCLUDE_CHECKBOX) }

    private val safeArguments get() = requireNotNull(arguments)

    internal val icon get() = safeArguments.getInt(KEY_TITLE_ICON, DEFAULT_VALUE)

    internal val isNotificationRequest get() = safeArguments.getBoolean(KEY_IS_NOTIFICATION_REQUEST, false)

    internal val positiveButtonBackgroundColor
        get() =
            safeArguments.getInt(KEY_POSITIVE_BUTTON_BACKGROUND_COLOR, DEFAULT_VALUE)

    internal val positiveButtonTextColor
        get() =
            safeArguments.getInt(KEY_POSITIVE_BUTTON_TEXT_COLOR, DEFAULT_VALUE)

    internal var userSelectionCheckBox: Boolean
        get() = safeArguments.getBoolean(KEY_USER_CHECK_BOX)
        set(value) {
            safeArguments.putBoolean(KEY_USER_CHECK_BOX, value)
        }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val sheetDialog = Dialog(requireContext())
        sheetDialog.requestWindowFeature(Window.FEATURE_NO_TITLE)
        sheetDialog.setCanceledOnTouchOutside(true)

        val rootView = createContainer()

        sheetDialog.setContainerView(rootView)

        sheetDialog.window?.apply {

            if (dialogGravity != DEFAULT_VALUE) {
                setGravity(dialogGravity)
            }

            if (dialogShouldWidthMatchParent) {
                setBackgroundDrawable(ColorDrawable(Color.TRANSPARENT))
                // This must be called after addContentView, or it won't fully fill to the edge.
                setLayout(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)
            }
        }

        return sheetDialog
    }

    private fun Dialog.setContainerView(rootView: View) {
        if (dialogShouldWidthMatchParent) {
            setContentView(rootView)
        } else {
            addContentView(
                rootView,
                LayoutParams(
                    LayoutParams.MATCH_PARENT,
                    LayoutParams.MATCH_PARENT
                )
            )
        }
    }

    private fun createContainer(): View {
        val rootView = LayoutInflater.from(requireContext()).inflate(
            R.layout.mozac_site_permissions_prompt,
            null,
            false
        )

        rootView.findViewById<TextView>(R.id.title).text = title
        rootView.findViewById<ImageView>(R.id.icon).setImageResource(icon)

        val positiveButton = rootView.findViewById<Button>(R.id.allow_button)
        val negativeButton = rootView.findViewById<Button>(R.id.deny_button)

        positiveButton.setOnClickListener {
            feature?.onPositiveButtonPress(sessionId, userSelectionCheckBox)
            dismiss()
        }

        if (positiveButtonBackgroundColor != DEFAULT_VALUE) {
            val backgroundTintList = ContextCompat.getColorStateList(requireContext(), positiveButtonBackgroundColor)
            positiveButton.backgroundTintList = backgroundTintList
        }

        if (positiveButtonTextColor != DEFAULT_VALUE) {
            val color = ContextCompat.getColor(requireContext(), positiveButtonTextColor)
            positiveButton.setTextColor(color)
        }

        negativeButton.setOnClickListener {
            feature?.onNegativeButtonPress(sessionId, userSelectionCheckBox)
            dismiss()
        }
        if (isNotificationRequest) {
            positiveButton.setText(R.string.mozac_feature_sitepermissions_always_allow)
            negativeButton.setText(R.string.mozac_feature_sitepermissions_never_allow)
        }

        if (shouldIncludeCheckBox) {
            addCheckbox(rootView)
        }

        return rootView
    }

    @SuppressLint("InflateParams")
    private fun addCheckbox(containerView: View) {
        val checkBox = containerView.findViewById<CheckBox>(R.id.do_not_ask_again)
        checkBox.visibility = VISIBLE
        checkBox.setOnCheckedChangeListener { _, isChecked ->
            userSelectionCheckBox = isChecked
        }
    }

    companion object {
        @Suppress("LongParameterList")
        fun newInstance(
            sessionId: String,
            title: String,
            titleIcon: Int,
            feature: SitePermissionsFeature,
            shouldIncludeCheckBox: Boolean,
            isNotificationRequest: Boolean = false
        ): SitePermissionsDialogFragment {

            val fragment = SitePermissionsDialogFragment()
            val arguments = fragment.arguments ?: Bundle()

            with(arguments) {
                putString(KEY_SESSION_ID, sessionId)
                putString(KEY_TITLE, title)
                putInt(KEY_TITLE_ICON, titleIcon)
                putBoolean(KEY_SHOULD_INCLUDE_CHECKBOX, shouldIncludeCheckBox)
                putBoolean(KEY_USER_CHECK_BOX, shouldIncludeCheckBox)
                putBoolean(KEY_IS_NOTIFICATION_REQUEST, isNotificationRequest)
                if (isNotificationRequest) {
                    putBoolean(KEY_USER_CHECK_BOX, false)
                    putBoolean(KEY_SHOULD_INCLUDE_CHECKBOX, false)
                }

                feature.promptsStyling?.apply {
                    putInt(KEY_DIALOG_GRAVITY, gravity)
                    putBoolean(KEY_DIALOG_WIDTH_MATCH_PARENT, shouldWidthMatchParent)

                    positiveButtonBackgroundColor?.apply {
                        putInt(KEY_POSITIVE_BUTTON_BACKGROUND_COLOR, this)
                    }

                    positiveButtonTextColor?.apply {
                        putInt(KEY_POSITIVE_BUTTON_TEXT_COLOR, this)
                    }
                }
            }
            fragment.feature = feature
            fragment.arguments = arguments
            return fragment
        }
    }
}
