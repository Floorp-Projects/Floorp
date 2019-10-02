/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts

import android.app.Activity
import android.content.Intent
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.PRIVATE
import androidx.fragment.app.Fragment
import androidx.fragment.app.FragmentManager
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.map
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.selector.findCustomTabOrSelectedTab
import mozilla.components.browser.state.selector.findTabOrCustomTab
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.prompt.Choice
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.concept.engine.prompt.PromptRequest.Alert
import mozilla.components.concept.engine.prompt.PromptRequest.Authentication
import mozilla.components.concept.engine.prompt.PromptRequest.Color
import mozilla.components.concept.engine.prompt.PromptRequest.File
import mozilla.components.concept.engine.prompt.PromptRequest.MenuChoice
import mozilla.components.concept.engine.prompt.PromptRequest.MultipleChoice
import mozilla.components.concept.engine.prompt.PromptRequest.SingleChoice
import mozilla.components.concept.engine.prompt.PromptRequest.TextPrompt
import mozilla.components.concept.engine.prompt.PromptRequest.TimeSelection
import mozilla.components.feature.prompts.ChoiceDialogFragment.Companion.MENU_CHOICE_DIALOG_TYPE
import mozilla.components.feature.prompts.ChoiceDialogFragment.Companion.MULTIPLE_CHOICE_DIALOG_TYPE
import mozilla.components.feature.prompts.ChoiceDialogFragment.Companion.SINGLE_CHOICE_DIALOG_TYPE
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.base.feature.OnNeedToRequestPermissions
import mozilla.components.support.base.feature.PermissionsFeature
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifAnyChanged
import java.security.InvalidParameterException
import java.util.Date

@VisibleForTesting(otherwise = PRIVATE)
internal const val FRAGMENT_TAG = "mozac_feature_prompt_dialog"

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
 * @property store The [BrowserStore] this feature should subscribe to.
 * @property customTabId Optional id of a custom tab. Instead of showing context
 * menus for the currently selected tab this feature will show only context menus
 * for this custom tab if an id is provided.
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
    private val store: BrowserStore,
    private var customTabId: String? = null,
    private val fragmentManager: FragmentManager,
    onNeedToRequestPermissions: OnNeedToRequestPermissions
) : LifecycleAwareFeature, PermissionsFeature {
    private var scope: CoroutineScope? = null
    private var activePromptRequest: PromptRequest? = null

    internal val promptAbuserDetector = PromptAbuserDetector()

    constructor(
        activity: Activity,
        store: BrowserStore,
        sessionId: String? = null,
        fragmentManager: FragmentManager,
        onNeedToRequestPermissions: OnNeedToRequestPermissions
    ) : this(
        activity, null, store, sessionId, fragmentManager, onNeedToRequestPermissions
    )
    constructor(
        fragment: Fragment,
        store: BrowserStore,
        sessionId: String? = null,
        fragmentManager: FragmentManager,
        onNeedToRequestPermissions: OnNeedToRequestPermissions
    ) : this(
        null, fragment, store, sessionId, fragmentManager, onNeedToRequestPermissions
    )

    init {
        if (activity == null && fragment == null) {
            throw IllegalStateException(
                "activity and fragment references " +
                    "must not be both null, at least one must be initialized."
            )
        }
    }

    private val context get() = activity ?: requireNotNull(fragment).requireContext()

    private val filePicker = if (activity != null) {
        FilePicker(activity, store, customTabId, onNeedToRequestPermissions)
    } else {
        FilePicker(requireNotNull(fragment), store, customTabId, onNeedToRequestPermissions)
    }

    override val onNeedToRequestPermissions
        get() = filePicker.onNeedToRequestPermissions

    /**
     * Starts observing the selected session to listen for prompt requests
     * and displays a dialog when needed.
     */
    override fun start() {
        promptAbuserDetector.resetJSAlertAbuseState()

        scope = store.flowScoped { flow ->
            flow.map { state -> state.findCustomTabOrSelectedTab(customTabId) }
                .ifAnyChanged {
                    arrayOf(it?.content?.promptRequest, it?.content?.loading)
                }
                .collect { state ->
                    state?.content?.let {
                        if (it.promptRequest != activePromptRequest) {
                            onPromptRequested(state)
                        } else if (!it.loading) {
                            promptAbuserDetector.resetJSAlertAbuseState()
                        }
                        activePromptRequest = it.promptRequest
                    }
                }
        }

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
        scope?.cancel()
    }

    /**
     * Notifies the feature of intent results for prompt requests handled by
     * other apps like file chooser requests.
     *
     * @param requestCode The code of the app that requested the intent.
     * @param intent The result of the request.
     */
    fun onActivityResult(requestCode: Int, resultCode: Int, intent: Intent?) {
        filePicker.onActivityResult(requestCode, resultCode, intent)
    }

    /**
     * Notifies the feature that the permissions request was completed. It will then
     * either process or dismiss the prompt request.
     *
     * @param permissions List of permission requested.
     * @param grantResults The grant results for the corresponding permissions
     * @see [onNeedToRequestPermissions].
     */
    override fun onPermissionsResult(permissions: Array<String>, grantResults: IntArray) {
        filePicker.onPermissionsResult(permissions, grantResults)
    }

    /**
     * Invoked when a native dialog needs to be shown.
     *
     * @param session The session which requested the dialog.
     */
    @VisibleForTesting(otherwise = PRIVATE)
    internal fun onPromptRequested(session: SessionState) {
        // Some requests are handle with intents
        session.content.promptRequest?.let { promptRequest ->
            when (promptRequest) {
                is File -> filePicker.handleFileRequest(promptRequest)
                else -> handleDialogsRequest(promptRequest, session)
            }
        }
    }

    /**
     * Invoked when a dialog is dismissed. This consumes the [PromptFeature]
     * value from the session indicated by [sessionId].
     *
     * @param sessionId this is the id of the session which requested the prompt.
     */
    internal fun onCancel(sessionId: String) {
        store.consumePromptFrom(sessionId) {
            when (it) {
                is PromptRequest.Dismissible -> it.onDismiss()
                is PromptRequest.Popup -> it.onDeny()
            }
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
        store.consumePromptFrom(sessionId) {
            when (it) {
                is TimeSelection -> it.onConfirm(value as Date)
                is Color -> it.onConfirm(value as String)
                is Alert -> {
                    val shouldNotShowMoreDialogs = value as Boolean
                    promptAbuserDetector.userWantsMoreDialogs(!shouldNotShowMoreDialogs)
                    it.onConfirm(!shouldNotShowMoreDialogs)
                }
                is SingleChoice -> it.onConfirm(value as Choice)
                is MenuChoice -> it.onConfirm(value as Choice)
                is PromptRequest.Popup -> it.onAllow()
                is MultipleChoice -> it.onConfirm(value as Array<Choice>)

                is Authentication -> {
                    val pair = value as Pair<String, String>
                    it.onConfirm(pair.first, pair.second)
                }

                is TextPrompt -> {
                    val pair = value as Pair<Boolean, String>

                    val shouldNotShowMoreDialogs = pair.first
                    promptAbuserDetector.userWantsMoreDialogs(!shouldNotShowMoreDialogs)
                    it.onConfirm(!shouldNotShowMoreDialogs, pair.second)
                }

                is PromptRequest.Confirm -> {
                    val pair = value as Pair<Boolean, MultiButtonDialogFragment.ButtonType>
                    val isCheckBoxChecked = pair.first
                    promptAbuserDetector.userWantsMoreDialogs(!isCheckBoxChecked)
                    when (pair.second) {
                        MultiButtonDialogFragment.ButtonType.POSITIVE ->
                            it.onConfirmPositiveButton(!isCheckBoxChecked)
                        MultiButtonDialogFragment.ButtonType.NEGATIVE ->
                            it.onConfirmNegativeButton(!isCheckBoxChecked)
                        MultiButtonDialogFragment.ButtonType.NEUTRAL ->
                            it.onConfirmNeutralButton(!isCheckBoxChecked)
                    }
                }
            }
        }
    }

    /**
     * Invoked when the user is requesting to clear the selected value from the dialog.
     * This consumes the [PromptFeature] value from the [Session] indicated by [sessionId].
     *
     * @param sessionId that requested to show the dialog.
     */
    internal fun onClear(sessionId: String) {
        store.consumePromptFrom(sessionId) {
            when (it) {
                is TimeSelection -> it.onClear()
            }
        }
    }

    /**
     * Re-attaches a fragment that is still visible but not linked to this feature anymore.
     */
    private fun reattachFragment(fragment: PromptDialogFragment) {
        val session = store.state.findTabOrCustomTab(fragment.sessionId)
        if (session?.content?.promptRequest == null) {
            fragmentManager.beginTransaction()
                .remove(fragment)
                .commitAllowingStateLoss()
            return
        }
        // Re-assign the feature instance so that the fragment can invoke us once the user makes a selection or cancels
        // the dialog.
        fragment.feature = this
    }

    @Suppress("ComplexMethod", "LongMethod")
    @VisibleForTesting(otherwise = PRIVATE)
    internal fun handleDialogsRequest(
        promptRequest: PromptRequest,
        session: SessionState
    ) {
        // Requests that are handled with dialogs
        val dialog = when (promptRequest) {

            is SingleChoice -> ChoiceDialogFragment.newInstance(
                promptRequest.choices,
                session.id, SINGLE_CHOICE_DIALOG_TYPE
            )

            is MultipleChoice -> ChoiceDialogFragment.newInstance(
                promptRequest.choices, session.id, MULTIPLE_CHOICE_DIALOG_TYPE
            )

            is MenuChoice -> ChoiceDialogFragment.newInstance(
                promptRequest.choices, session.id, MENU_CHOICE_DIALOG_TYPE
            )

            is Alert -> {
                with(promptRequest) {
                    AlertDialogFragment.newInstance(
                        session.id,
                        title,
                        message,
                        promptAbuserDetector.areDialogsBeingAbused()
                    )
                }
            }

            is TimeSelection -> {

                val selectionType = when (promptRequest.type) {
                    TimeSelection.Type.DATE -> TimePickerDialogFragment.SELECTION_TYPE_DATE
                    TimeSelection.Type.DATE_AND_TIME -> TimePickerDialogFragment.SELECTION_TYPE_DATE_AND_TIME
                    TimeSelection.Type.TIME -> TimePickerDialogFragment.SELECTION_TYPE_TIME
                    TimeSelection.Type.MONTH -> TimePickerDialogFragment.SELECTION_TYPE_MONTH
                }

                with(promptRequest) {
                    TimePickerDialogFragment.newInstance(
                        session.id,
                        initialDate,
                        minimumDate,
                        maximumDate,
                        selectionType
                    )
                }
            }

            is TextPrompt -> {
                with(promptRequest) {
                    TextPromptDialogFragment.newInstance(
                        session.id,
                        title,
                        inputLabel,
                        inputValue,
                        promptAbuserDetector.areDialogsBeingAbused()
                    )
                }
            }

            is Authentication -> {
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

            is Color -> ColorPickerDialogFragment.newInstance(
                session.id,
                promptRequest.defaultColor
            )

            is PromptRequest.Popup -> {

                val title = context.getString(R.string.mozac_feature_prompts_popup_dialog_title)
                val positiveLabel = context.getString(R.string.mozac_feature_prompts_allow)
                val negativeLabel = context.getString(R.string.mozac_feature_prompts_deny)

                ConfirmDialogFragment.newInstance(
                    sessionId = session.id,
                    title = title,
                    message = promptRequest.targetUri,
                    positiveButtonText = positiveLabel,
                    negativeButtonText = negativeLabel
                )
            }

            is PromptRequest.Confirm -> {
                with(promptRequest) {
                    val positiveButton = if (positiveButtonTitle.isEmpty()) {
                        context.getString(R.string.mozac_feature_prompts_ok)
                    } else {
                        positiveButtonTitle
                    }
                    val negativeButton = if (positiveButtonTitle.isEmpty()) {
                        context.getString(R.string.mozac_feature_prompts_cancel)
                    } else {
                        positiveButtonTitle
                    }

                    MultiButtonDialogFragment.newInstance(
                        session.id,
                        title,
                        message,
                        promptAbuserDetector.areDialogsBeingAbused(),
                        positiveButton,
                        negativeButton,
                        neutralButtonTitle
                    )
                }
            }

            else -> throw InvalidParameterException("Not valid prompt request type")
        }

        dialog.feature = this

        if (canShowThisPrompt(promptRequest)) {
            dialog.show(fragmentManager, FRAGMENT_TAG)
        } else {
            (promptRequest as PromptRequest.Dismissible).onDismiss()
            store.dispatch(ContentAction.ConsumePromptRequestAction(session.id))
        }
        promptAbuserDetector.updateJSDialogAbusedState()
    }

    private fun canShowThisPrompt(promptRequest: PromptRequest): Boolean {
        return when (promptRequest) {
            is SingleChoice,
            is MultipleChoice,
            is MenuChoice,
            is TimeSelection,
            is File,
            is Color,
            is Authentication,
            is PromptRequest.Popup -> true
            is Alert, is TextPrompt, is PromptRequest.Confirm -> promptAbuserDetector.shouldShowMoreDialogs
        }
    }

    companion object {
        const val FILE_PICKER_ACTIVITY_REQUEST_CODE = 1234
    }
}

internal fun BrowserStore.consumePromptFrom(
    sessionId: String?,
    consume: (PromptRequest) -> Unit
) {
    if (sessionId == null) {
        state.selectedTab
    } else {
        state.findTabOrCustomTab(sessionId)
    }?.let { tab ->
        tab.content.promptRequest?.let {
            consume(it)
            dispatch(ContentAction.ConsumePromptRequestAction(tab.id))
        }
    }
}
