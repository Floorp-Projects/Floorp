/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts

import android.Manifest.permission.READ_EXTERNAL_STORAGE
import android.app.Activity
import android.app.Activity.RESULT_OK
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.support.annotation.VisibleForTesting
import android.support.annotation.VisibleForTesting.PRIVATE
import android.support.v4.app.Fragment
import android.support.v4.app.FragmentManager
import mozilla.components.browser.session.SelectionAwareSessionObserver
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.prompt.Choice
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.concept.engine.prompt.PromptRequest.TextPrompt
import mozilla.components.concept.engine.prompt.PromptRequest.Color
import mozilla.components.concept.engine.prompt.PromptRequest.Authentication
import mozilla.components.concept.engine.prompt.PromptRequest.Alert
import mozilla.components.concept.engine.prompt.PromptRequest.MultipleChoice
import mozilla.components.concept.engine.prompt.PromptRequest.SingleChoice
import mozilla.components.concept.engine.prompt.PromptRequest.MenuChoice
import mozilla.components.concept.engine.prompt.PromptRequest.File
import mozilla.components.feature.prompts.ChoiceDialogFragment.Companion.MULTIPLE_CHOICE_DIALOG_TYPE
import mozilla.components.feature.prompts.ChoiceDialogFragment.Companion.MENU_CHOICE_DIALOG_TYPE
import mozilla.components.feature.prompts.ChoiceDialogFragment.Companion.SINGLE_CHOICE_DIALOG_TYPE
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.ktx.android.content.isPermissionGranted
import java.security.InvalidParameterException
import mozilla.components.concept.engine.prompt.PromptRequest.TimeSelection

import java.util.Date

@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
internal const val FRAGMENT_TAG = "mozac_feature_prompt_dialog"

typealias OnNeedToRequestPermissions = (permissions: Array<String>) -> Unit

/**
 * Feature for displaying native dialogs for html elements like: input type
 * date, file, time, color, option, menu, authentication, confirmation and alerts.
 *
 * There are some requests that are handled with intents instead of dialogs,
 * like file choosers and others. For this reason, you have to keep the feature
 * aware of the flow of requesting data from other apps, overriding
 * onActivityResult in your [Activity] or [Fragment] and forward its calls
 * to [onActivityResult].
 *
 * This feature will subscribe to the currently selected [Session] and display
 * a suitable native dialog based on [Session.Observer.onPromptRequested] events.
 * Once the dialog is closed or the user selects an item from the dialog
 * the related [PromptRequest] will be consumed.
 *
 * @property activity The [Activity] which hosts this feature. If hosted by a
 * [Fragment], this parameter can be ignored. Note that an
 * [IllegalStateException] will be thrown if neither an active nor a fragment
 * is specified.
 * @property fragment The [Fragment] which hosts this feature. If hosted by an
 * [Activity], this parameter can be ignored. Note that an
 * [IllegalStateException] will be thrown if neither an active nor a fragment
 * is specified.
 * @property sessionManager The [SessionManager] instance in order to subscribe
 * to the selected [Session].
 * @property fragmentManager The [FragmentManager] to be used when displaying
 * a dialog (fragment).
 * @property onNeedToRequestPermissions a callback invoked when permissions
 * need to be requested before a prompt (e.g. a file picker) can be displayed.
 * Once the request is completed, [onPermissionsResult] needs to be invoked.
 */

@Suppress("TooManyFunctions")
class PromptFeature(
    private val activity: Activity? = null,
    private val fragment: Fragment? = null,
    private val sessionManager: SessionManager,
    private val fragmentManager: FragmentManager,
    private val onNeedToRequestPermissions: OnNeedToRequestPermissions

) : LifecycleAwareFeature {

    init {
        if (activity == null && fragment == null) {
            throw IllegalStateException(
                "activity and fragment references " +
                    "must not be both null, at least one must be initialized."
            )
        }
    }

    private val observer = PromptRequestObserver(sessionManager, feature = this)

    private val context get() = activity ?: requireNotNull(fragment).requireContext()

    /**
     * Starts observing the selected session to listen for prompt requests
     * and displays a dialog when needed.
     */
    override fun start() {
        observer.observeSelected()

        fragmentManager.findFragmentByTag(FRAGMENT_TAG)?.let { fragment ->
            // There's still a [PromptDialogFragment] visible from the last time. Re-attach this feature so that the
            // fragment can invoke the callback on this feature once the user makes a selection. This can happen when
            // the app was in the background and on resume the activity and fragments get recreated.
            reattachFragment(fragment as PromptDialogFragment)
        }
    }

    /**
     * Stops observing the selected session for incoming prompt requests.
     */
    override fun stop() {
        observer.stop()
    }

    /**
     * Notifies the feature of intent results for prompt requests handled by
     * other apps like file chooser requests.
     *
     * @param requestCode The code of the app that requested the intent.
     * @param intent The result of the request.
     */
    fun onActivityResult(requestCode: Int, resultCode: Int, intent: Intent?) {
        if (requestCode == FILE_PICKER_ACTIVITY_REQUEST_CODE) {
            sessionManager.selectedSession?.promptRequest?.consume {

                val request = it as File

                if (resultCode != RESULT_OK || intent == null) {
                    request.onDismiss()
                } else {
                    handleFilePickerIntentResult(intent, request)
                }
                true
            }
        }
    }

    /**
     * Notifies the feature that the permissions request was completed. It will then
     * either process or dismiss the prompt request.
     *
     * @param permissions List of permission requested.
     * @param grantResults The grant results for the corresponding permissions
     * @see [onNeedToRequestPermissions].
     */
    fun onPermissionsResult(permissions: Array<String>, grantResults: IntArray) {
        if (grantResults.isNotEmpty() && grantResults.all { it == PackageManager.PERMISSION_GRANTED }) {
            onPermissionsGranted()
        } else {
            onPermissionsDenied()
        }
    }

    /**
     * Used in conjunction with [onNeedToRequestPermissions], to notify the feature
     * that all the required permissions have been granted, and the pending [PromptRequest]
     * can be performed.
     *
     * If the required permission has not been granted
     * [onNeedToRequestPermissions] will be called.
     */
    @VisibleForTesting(otherwise = PRIVATE)
    internal fun onPermissionsGranted() {
        sessionManager.selectedSession?.apply {
            promptRequest.consume { promptRequest ->
                onPromptRequested(this, promptRequest)
                false
            }
        }
    }

    /**
     * Used in conjunction with [onNeedToRequestPermissions] to notify the feature that one
     * or more required permissions have been denied.
     */
    @VisibleForTesting(otherwise = PRIVATE)
    internal fun onPermissionsDenied() {
        sessionManager.selectedSession?.apply {
            promptRequest.consume { request ->
                if (request is File) {
                    request.onDismiss()
                }
                true
            }
        }
    }

    @VisibleForTesting(otherwise = PRIVATE)
    internal fun handleFilePickerIntentResult(
        intent: Intent,
        request: File
    ) {
        intent.apply {

            if (request.isMultipleFilesSelection) {
                handleMultipleFileSelections(request, this)
            } else {
                handleSingleFileSelection(request, this)
            }
        }
    }

    @VisibleForTesting(otherwise = PRIVATE)
    internal fun handleSingleFileSelection(
        request: File,
        intent: Intent
    ) {
        intent.data?.apply {
            request.onSingleFileSelected(context, this)
        }
    }

    @VisibleForTesting(otherwise = PRIVATE)
    internal fun handleMultipleFileSelections(
        request: File,
        intent: Intent
    ) {
        intent.clipData?.apply {
            val uris = Array<Uri>(itemCount) { index -> getItemAt(index).uri }
            request.onMultipleFilesSelected(context, uris)
        }
    }

    /**
     * Invoked when a native dialog needs to be shown.
     * Displays a suitable dialog for the pending [promptRequest].
     *
     * @param session The session which requested the dialog.
     * @param promptRequest The session the request the dialog.
     */
    @VisibleForTesting(otherwise = PRIVATE)
    internal fun onPromptRequested(session: Session, promptRequest: PromptRequest) {

        // Requests that are handle with intents
        when (promptRequest) {
            is File -> {
                handleFilePickerRequest(promptRequest)
                return
            }
        }
        handleDialogsRequest(promptRequest, session)
    }

    /**
     * Invoked when a dialog is dismissed. This consumes the [PromptFeature]
     * value from the [Session] indicated by [sessionId].
     *
     * @param sessionId this is the id of the session which requested the prompt.
     */
    internal fun onCancel(sessionId: String) {
        val session = sessionManager.findSessionById(sessionId) ?: return
        session.promptRequest.consume {
            when (it) {
                is Alert -> it.onDismiss()
                is Authentication -> it.onDismiss()
                is Color -> it.onDismiss()
                is TextPrompt -> it.onDismiss()
                is PromptRequest.Popup -> it.onDeny()
                is PromptRequest.Confirm -> it.onDismiss()
            }
            true
        }
    }

    /**
     * Invoked when the user confirms the action on the dialog. This consumes
     * the [PromptFeature] value from the [Session] indicated by [sessionId].
     *
     * @param sessionId that requested to show the dialog.
     * @param value an optional value provided by the dialog as a result of confirming the action.
     */
    @Suppress("UNCHECKED_CAST", "ComplexMethod")
    internal fun onConfirm(sessionId: String, value: Any? = null) {
        val session = sessionManager.findSessionById(sessionId) ?: return
        session.promptRequest.consume {
            when (it) {
                is TimeSelection -> it.onConfirm(value as Date)
                is Color -> it.onConfirm(value as String)
                is Alert -> it.onConfirm(value as Boolean)
                is SingleChoice -> it.onConfirm(value as Choice)
                is MenuChoice -> it.onConfirm(value as Choice)
                is PromptRequest.Popup -> it.onAllow()

                is MultipleChoice -> {
                    it.onConfirm(value as Array<Choice>)
                }

                is Authentication -> {
                    val pair = value as Pair<String, String>
                    it.onConfirm(pair.first, pair.second)
                }

                is TextPrompt -> {
                    val pair = value as Pair<Boolean, String>
                    it.onConfirm(pair.first, pair.second)
                }

                is PromptRequest.Confirm -> {
                    val pair = value as Pair<Boolean, MultiButtonDialogFragment.ButtonType>
                    when (pair.second) {

                        MultiButtonDialogFragment.ButtonType.POSITIVE -> {
                            it.onConfirmPositiveButton(pair.first)
                        }
                        MultiButtonDialogFragment.ButtonType.NEGATIVE -> {
                            it.onConfirmNegativeButton(pair.first)
                        }
                        MultiButtonDialogFragment.ButtonType.NEUTRAL -> {
                            it.onConfirmNeutralButton(pair.first)
                        }
                    }
                }
            }
            true
        }
    }

    /**
     * Invoked when the user is requesting to clear the selected value from the dialog.
     * This consumes the [PromptFeature] value from the [Session] indicated by [sessionId].
     *
     * @param sessionId that requested to show the dialog.
     */
    internal fun onClear(sessionId: String) {
        val session = sessionManager.findSessionById(sessionId) ?: return
        session.promptRequest.consume {
            when (it) {
                is PromptRequest.TimeSelection -> it.onClear()
            }
            true
        }
    }

    /**
     * Re-attaches a fragment that is still visible but not linked to this feature anymore.
     */
    private fun reattachFragment(fragment: PromptDialogFragment) {
        val session = sessionManager.findSessionById(fragment.sessionId)
        if (session == null || session.promptRequest.isConsumed()) {
            fragmentManager.beginTransaction()
                .remove(fragment)
                .commitAllowingStateLoss()
            return
        }
        // Re-assign the feature instance so that the fragment can invoke us once the user makes a selection or cancels
        // the dialog.
        fragment.feature = this
    }

    internal fun handleFilePickerRequest(promptRequest: File) {
        if (context.isPermissionGranted(READ_EXTERNAL_STORAGE)) {
            val intent = buildFileChooserIntent(
                promptRequest.isMultipleFilesSelection,
                promptRequest.mimeTypes
            )
            startActivityForResult(intent, FILE_PICKER_ACTIVITY_REQUEST_CODE)
        } else {
            onNeedToRequestPermissions(arrayOf(READ_EXTERNAL_STORAGE))
        }
    }

    internal fun startActivityForResult(intent: Intent, code: Int) {
        if (activity != null) {
            activity.startActivityForResult(intent, code)
        } else {
            requireNotNull(fragment).startActivityForResult(intent, code)
        }
    }

    internal fun buildFileChooserIntent(allowMultipleSelection: Boolean, mimeTypes: Array<out String>): Intent {
        return with(Intent(Intent.ACTION_GET_CONTENT)) {
            type = "*/*"
            addCategory(Intent.CATEGORY_OPENABLE)
            putExtra(Intent.EXTRA_LOCAL_ONLY, true)
            if (mimeTypes.isNotEmpty()) {
                putExtra(Intent.EXTRA_MIME_TYPES, mimeTypes)
            }
            putExtra(Intent.EXTRA_ALLOW_MULTIPLE, allowMultipleSelection)
        }
    }

    @Suppress("ComplexMethod")
    @VisibleForTesting(otherwise = PRIVATE)
    internal fun handleDialogsRequest(
        promptRequest: PromptRequest,
        session: Session
    ) {
        // Requests that are handled with dialogs
        val dialog = when (promptRequest) {

            is SingleChoice -> {
                ChoiceDialogFragment.newInstance(
                    promptRequest.choices,
                    session.id, SINGLE_CHOICE_DIALOG_TYPE
                )
            }

            is MultipleChoice -> ChoiceDialogFragment.newInstance(
                promptRequest.choices, session.id, MULTIPLE_CHOICE_DIALOG_TYPE
            )

            is MenuChoice -> ChoiceDialogFragment.newInstance(
                promptRequest.choices, session.id, MENU_CHOICE_DIALOG_TYPE
            )

            is Alert -> {
                with(promptRequest) {
                    AlertDialogFragment.newInstance(session.id, title, message, hasShownManyDialogs)
                }
            }

            is PromptRequest.TimeSelection -> {

                val selectionType = when (promptRequest.type) {
                    TimeSelection.Type.DATE -> TimePickerDialogFragment.SELECTION_TYPE_DATE
                    TimeSelection.Type.DATE_AND_TIME -> TimePickerDialogFragment.SELECTION_TYPE_DATE_AND_TIME
                    TimeSelection.Type.TIME -> TimePickerDialogFragment.SELECTION_TYPE_TIME
                }

                with(promptRequest) {
                    TimePickerDialogFragment.newInstance(
                        session.id,
                        title,
                        initialDate,
                        minimumDate,
                        maximumDate,
                        selectionType
                    )
                }
            }

            is PromptRequest.TextPrompt -> {
                with(promptRequest) {
                    TextPromptDialogFragment.newInstance(session.id, title, inputLabel, inputValue, hasShownManyDialogs)
                }
            }

            is PromptRequest.Authentication -> {
                with(promptRequest) {
                    AuthenticationDialogFragment.newInstance(
                        session.id,
                        title,
                        message,
                        userName,
                        password,
                        onlyShowPassword
                    )
                }
            }

            is PromptRequest.Color -> {
                with(promptRequest) {
                    ColorPickerDialogFragment.newInstance(session.id, defaultColor)
                }
            }

            is PromptRequest.Popup -> {
                val title = context.getString(R.string.mozac_feature_prompts_popup_dialog_title)
                val positiveLabel = context.getString(R.string.mozac_feature_prompts_allow)
                val negativeLabel = context.getString(R.string.mozac_feature_prompts_deny)

                with(promptRequest) {
                    ConfirmDialogFragment.newInstance(
                        sessionId = session.id,
                        title = title,
                        message = targetUri,
                        positiveButtonText = positiveLabel,
                        negativeButtonText = negativeLabel
                    )
                }
            }

            is PromptRequest.Confirm -> {

                with(promptRequest) {
                    MultiButtonDialogFragment.newInstance(
                        session.id,
                        title,
                        message,
                        hasShownManyDialogs,
                        positiveButtonTitle,
                        negativeButtonTitle,
                        neutralButtonTitle
                    )
                }
            }

            else -> {
                throw InvalidParameterException("Not valid prompt request type")
            }
        }

        dialog.feature = this
        dialog.show(fragmentManager, FRAGMENT_TAG)
    }

    /**
     * Observes [Session.Observer.onPromptRequested] of the selected session
     * and notifies the feature whenever a prompt needs to be shown.
     */
    internal class PromptRequestObserver(
        sessionManager: SessionManager,
        private val feature: PromptFeature
    ) : SelectionAwareSessionObserver(sessionManager) {

        override fun onPromptRequested(session: Session, promptRequest: PromptRequest): Boolean {
            feature.onPromptRequested(session, promptRequest)
            return false
        }
    }

    companion object {
        const val FILE_PICKER_ACTIVITY_REQUEST_CODE = 1234
    }
}
