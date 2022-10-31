/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.sitepermissions

import android.annotation.SuppressLint
import android.app.Dialog
import android.content.DialogInterface
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.View.VISIBLE
import android.view.ViewGroup
import android.view.Window
import android.widget.Button
import android.widget.CheckBox
import android.widget.ImageView
import android.widget.LinearLayout.LayoutParams
import android.widget.TextView
import androidx.appcompat.app.AppCompatDialogFragment
import androidx.core.content.ContextCompat

internal const val KEY_SESSION_ID = "KEY_SESSION_ID"
internal const val KEY_TITLE = "KEY_TITLE"
private const val KEY_DIALOG_GRAVITY = "KEY_DIALOG_GRAVITY"
private const val KEY_DIALOG_WIDTH_MATCH_PARENT = "KEY_DIALOG_WIDTH_MATCH_PARENT"
private const val KEY_TITLE_ICON = "KEY_TITLE_ICON"
private const val KEY_MESSAGE = "KEY_MESSAGE"
private const val KEY_NEGATIVE_BUTTON_TEXT = "KEY_NEGATIVE_BUTTON_TEXT"
private const val KEY_POSITIVE_BUTTON_BACKGROUND_COLOR = "KEY_POSITIVE_BUTTON_BACKGROUND_COLOR"
private const val KEY_POSITIVE_BUTTON_TEXT_COLOR = "KEY_POSITIVE_BUTTON_TEXT_COLOR"
private const val KEY_SHOULD_SHOW_LEARN_MORE_LINK = "KEY_SHOULD_SHOW_LEARN_MORE_LINK"
private const val KEY_SHOULD_SHOW_DO_NOT_ASK_AGAIN_CHECKBOX = "KEY_SHOULD_SHOW_DO_NOT_ASK_AGAIN_CHECKBOX"
private const val KEY_SHOULD_PRESELECT_DO_NOT_ASK_AGAIN_CHECKBOX = "KEY_SHOULD_PRESELECT_DO_NOT_ASK_AGAIN_CHECKBOX"
private const val KEY_IS_NOTIFICATION_REQUEST = "KEY_IS_NOTIFICATION_REQUEST"
private const val DEFAULT_VALUE = Int.MAX_VALUE
private const val KEY_PERMISSION_ID = "KEY_PERMISSION_ID"

internal open class SitePermissionsDialogFragment : AppCompatDialogFragment() {

    // Safe Arguments

    private val safeArguments get() = requireNotNull(arguments)

    internal val sessionId: String get() =
        safeArguments.getString(KEY_SESSION_ID, "")
    internal val title: String get() =
        safeArguments.getString(KEY_TITLE, "")
    internal val icon get() =
        safeArguments.getInt(KEY_TITLE_ICON, DEFAULT_VALUE)
    internal val message: String? get() =
        safeArguments.getString(KEY_MESSAGE, null)
    internal val negativeButtonText: String? get() =
        safeArguments.getString(KEY_NEGATIVE_BUTTON_TEXT, null)

    internal val dialogGravity: Int get() =
        safeArguments.getInt(KEY_DIALOG_GRAVITY, DEFAULT_VALUE)
    internal val dialogShouldWidthMatchParent: Boolean get() =
        safeArguments.getBoolean(KEY_DIALOG_WIDTH_MATCH_PARENT)

    internal val positiveButtonBackgroundColor get() =
        safeArguments.getInt(KEY_POSITIVE_BUTTON_BACKGROUND_COLOR, DEFAULT_VALUE)
    internal val positiveButtonTextColor get() =
        safeArguments.getInt(KEY_POSITIVE_BUTTON_TEXT_COLOR, DEFAULT_VALUE)

    internal val isNotificationRequest get() =
        safeArguments.getBoolean(KEY_IS_NOTIFICATION_REQUEST, false)

    internal val shouldShowLearnMoreLink: Boolean get() =
        safeArguments.getBoolean(KEY_SHOULD_SHOW_LEARN_MORE_LINK, false)
    internal val shouldShowDoNotAskAgainCheckBox: Boolean get() =
        safeArguments.getBoolean(KEY_SHOULD_SHOW_DO_NOT_ASK_AGAIN_CHECKBOX, true)
    internal val shouldPreselectDoNotAskAgainCheckBox: Boolean get() =
        safeArguments.getBoolean(KEY_SHOULD_PRESELECT_DO_NOT_ASK_AGAIN_CHECKBOX, false)
    internal val permissionRequestId: String get() =
        safeArguments.getString(KEY_PERMISSION_ID, "")

    // State

    internal var feature: SitePermissionsFeature? = null
    internal var userSelectionCheckBox: Boolean = false

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        userSelectionCheckBox = shouldPreselectDoNotAskAgainCheckBox

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

    override fun onDismiss(dialog: DialogInterface) {
        super.onDismiss(dialog)
        feature?.onDismiss(permissionRequestId, sessionId)
    }

    private fun Dialog.setContainerView(rootView: View) {
        if (dialogShouldWidthMatchParent) {
            setContentView(rootView)
        } else {
            addContentView(
                rootView,
                LayoutParams(
                    LayoutParams.MATCH_PARENT,
                    LayoutParams.MATCH_PARENT,
                ),
            )
        }
    }

    @SuppressLint("InflateParams")
    private fun createContainer(): View {
        val rootView = LayoutInflater.from(requireContext()).inflate(
            R.layout.mozac_site_permissions_prompt,
            null,
            false,
        )

        rootView.findViewById<TextView>(R.id.title).text = title
        rootView.findViewById<ImageView>(R.id.icon).setImageResource(icon)
        message?.let {
            rootView.findViewById<TextView>(R.id.message).apply {
                visibility = VISIBLE
                text = it
            }
        }
        if (shouldShowLearnMoreLink) {
            rootView.findViewById<TextView>(R.id.learn_more).apply {
                visibility = VISIBLE
                isLongClickable = false
                setOnClickListener {
                    dismiss()
                    feature?.onLearnMorePress(permissionRequestId, sessionId)
                }
            }
        }

        val positiveButton = rootView.findViewById<Button>(R.id.allow_button)
        val negativeButton = rootView.findViewById<Button>(R.id.deny_button)

        positiveButton.setOnClickListener {
            feature?.onPositiveButtonPress(permissionRequestId, sessionId, userSelectionCheckBox)
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
            feature?.onNegativeButtonPress(permissionRequestId, sessionId, userSelectionCheckBox)
            dismiss()
        }
        if (isNotificationRequest) {
            positiveButton.setText(R.string.mozac_feature_sitepermissions_always_allow)
            negativeButton.setText(R.string.mozac_feature_sitepermissions_never_allow)
        }
        negativeButtonText?.let {
            negativeButton.text = it
        }

        if (shouldShowDoNotAskAgainCheckBox) {
            showDoNotAskAgainCheckbox(rootView, checked = shouldPreselectDoNotAskAgainCheckBox)
        }

        return rootView
    }

    private fun showDoNotAskAgainCheckbox(containerView: View, checked: Boolean) {
        containerView.findViewById<CheckBox>(R.id.do_not_ask_again).apply {
            visibility = VISIBLE
            isChecked = checked
            setOnCheckedChangeListener { _, isChecked ->
                userSelectionCheckBox = isChecked
            }
        }
    }

    companion object {
        @Suppress("LongParameterList")
        fun newInstance(
            sessionId: String,
            title: String,
            titleIcon: Int,
            permissionRequestId: String = "",
            feature: SitePermissionsFeature,
            shouldShowDoNotAskAgainCheckBox: Boolean,
            shouldSelectDoNotAskAgainCheckBox: Boolean = false,
            isNotificationRequest: Boolean = false,
            message: String? = null,
            negativeButtonText: String? = null,
            shouldShowLearnMoreLink: Boolean = false,
        ): SitePermissionsDialogFragment {
            val fragment = SitePermissionsDialogFragment()
            val arguments = fragment.arguments ?: Bundle()

            arguments.apply {
                putString(KEY_SESSION_ID, sessionId)
                putString(KEY_TITLE, title)
                putInt(KEY_TITLE_ICON, titleIcon)
                putString(KEY_MESSAGE, message)
                putString(KEY_NEGATIVE_BUTTON_TEXT, negativeButtonText)
                putString(KEY_PERMISSION_ID, permissionRequestId)
                putBoolean(KEY_SHOULD_SHOW_LEARN_MORE_LINK, shouldShowLearnMoreLink)

                putBoolean(KEY_IS_NOTIFICATION_REQUEST, isNotificationRequest)
                if (isNotificationRequest) {
                    putBoolean(KEY_SHOULD_SHOW_DO_NOT_ASK_AGAIN_CHECKBOX, false)
                    putBoolean(KEY_SHOULD_PRESELECT_DO_NOT_ASK_AGAIN_CHECKBOX, true)
                } else {
                    putBoolean(KEY_SHOULD_SHOW_DO_NOT_ASK_AGAIN_CHECKBOX, shouldShowDoNotAskAgainCheckBox)
                    putBoolean(KEY_SHOULD_PRESELECT_DO_NOT_ASK_AGAIN_CHECKBOX, shouldSelectDoNotAskAgainCheckBox)
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
