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
import kotlinx.coroutines.flow.filter
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.mapNotNull
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.selector.findTabOrCustomTab
import mozilla.components.browser.state.selector.findTabOrCustomTabOrSelectedTab
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.prompt.Choice
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.concept.engine.prompt.PromptRequest.Alert
import mozilla.components.concept.engine.prompt.PromptRequest.Authentication
import mozilla.components.concept.engine.prompt.PromptRequest.BeforeUnload
import mozilla.components.concept.engine.prompt.PromptRequest.Color
import mozilla.components.concept.engine.prompt.PromptRequest.Confirm
import mozilla.components.concept.engine.prompt.PromptRequest.Dismissible
import mozilla.components.concept.engine.prompt.PromptRequest.File
import mozilla.components.concept.engine.prompt.PromptRequest.MenuChoice
import mozilla.components.concept.engine.prompt.PromptRequest.MultipleChoice
import mozilla.components.concept.engine.prompt.PromptRequest.Popup
import mozilla.components.concept.engine.prompt.PromptRequest.SaveLoginPrompt
import mozilla.components.concept.engine.prompt.PromptRequest.SelectLoginPrompt
import mozilla.components.concept.engine.prompt.PromptRequest.Share
import mozilla.components.concept.engine.prompt.PromptRequest.SingleChoice
import mozilla.components.concept.engine.prompt.PromptRequest.TextPrompt
import mozilla.components.concept.engine.prompt.PromptRequest.TimeSelection
import mozilla.components.concept.storage.Login
import mozilla.components.concept.storage.LoginValidationDelegate
import mozilla.components.feature.prompts.dialog.AlertDialogFragment
import mozilla.components.feature.prompts.dialog.AuthenticationDialogFragment
import mozilla.components.feature.prompts.dialog.ChoiceDialogFragment
import mozilla.components.feature.prompts.dialog.ChoiceDialogFragment.Companion.MENU_CHOICE_DIALOG_TYPE
import mozilla.components.feature.prompts.dialog.ChoiceDialogFragment.Companion.MULTIPLE_CHOICE_DIALOG_TYPE
import mozilla.components.feature.prompts.dialog.ChoiceDialogFragment.Companion.SINGLE_CHOICE_DIALOG_TYPE
import mozilla.components.feature.prompts.dialog.ColorPickerDialogFragment
import mozilla.components.feature.prompts.dialog.ConfirmDialogFragment
import mozilla.components.feature.prompts.dialog.MultiButtonDialogFragment
import mozilla.components.feature.prompts.dialog.PromptAbuserDetector
import mozilla.components.feature.prompts.dialog.PromptDialogFragment
import mozilla.components.feature.prompts.dialog.Prompter
import mozilla.components.feature.prompts.dialog.SaveLoginDialogFragment
import mozilla.components.feature.prompts.dialog.TextPromptDialogFragment
import mozilla.components.feature.prompts.dialog.TimePickerDialogFragment
import mozilla.components.feature.prompts.file.FilePicker
import mozilla.components.feature.prompts.login.LoginExceptions
import mozilla.components.feature.prompts.login.LoginPicker
import mozilla.components.feature.prompts.login.LoginPickerView
import mozilla.components.feature.prompts.share.DefaultShareDelegate
import mozilla.components.feature.prompts.share.ShareDelegate
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.base.feature.OnNeedToRequestPermissions
import mozilla.components.support.base.feature.PermissionsFeature
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifAnyChanged
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged
import java.lang.ref.WeakReference
import java.security.InvalidParameterException
import java.util.Date

@VisibleForTesting(otherwise = PRIVATE)
internal const val FRAGMENT_TAG = "mozac_feature_prompt_dialog"

private const val PROGRESS_ALMOST_COMPLETE = 90

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
 * This feature will subscribe to the currently selected session and display
 * a suitable native dialog based on [Session.Observer.onPromptRequested] events.
 * Once the dialog is closed or the user selects an item from the dialog
 * the related [PromptRequest] will be consumed.
 *
 * @property container The [Activity] or [Fragment] which hosts this feature.
 * @property store The [BrowserStore] this feature should subscribe to.
 * @property customTabId Optional id of a custom tab. Instead of showing context
 * menus for the currently selected tab this feature will show only context menus
 * for this custom tab if an id is provided.
 * @property fragmentManager The [FragmentManager] to be used when displaying
 * a dialog (fragment).
 * @property shareDelegate Delegate used to display share sheet.
 * @property loginStorageDelegate Delegate used to access login storage. If null,
 * 'save login'prompts will not be shown.
 * @property isSaveLoginEnabled A callback invoked when a login prompt is triggered. If false,
 * 'save login'prompts will not be shown.
 * @property loginExceptionStorage An implementation of [LoginExceptions] that saves and checks origins
 * the user does not want to see a save login dialog for.
 * @property loginPickerView The [LoginPickerView] used for [LoginPicker] to display select login options.
 * @property onManageLogins A callback invoked when a user selects "manage logins" from the
 * select login prompt.
 * @property onNeedToRequestPermissions A callback invoked when permissions
 * need to be requested before a prompt (e.g. a file picker) can be displayed.
 * Once the request is completed, [onPermissionsResult] needs to be invoked.
 */
@Suppress("TooManyFunctions", "LargeClass", "LongParameterList")
class PromptFeature private constructor(
    private val container: PromptContainer,
    private val store: BrowserStore,
    private var customTabId: String?,
    private val fragmentManager: FragmentManager,
    private val shareDelegate: ShareDelegate,
    override val loginValidationDelegate: LoginValidationDelegate? = null,
    private val isSaveLoginEnabled: () -> Boolean = { false },
    override val loginExceptionStorage: LoginExceptions? = null,
    private val loginPickerView: LoginPickerView? = null,
    private val onManageLogins: () -> Unit = {},
    onNeedToRequestPermissions: OnNeedToRequestPermissions
) : LifecycleAwareFeature, PermissionsFeature, Prompter {
    // These three scopes have identical lifetimes. We do not yet have a way of combining scopes
    private var handlePromptScope: CoroutineScope? = null
    private var dismissPromptScope: CoroutineScope? = null
    private var sessionPromptScope: CoroutineScope? = null
    private var activePromptRequest: PromptRequest? = null

    internal val promptAbuserDetector = PromptAbuserDetector()
    private val logger = Logger("PromptFeature")

    @VisibleForTesting(otherwise = PRIVATE)
    internal var activePrompt: WeakReference<PromptDialogFragment>? = null

    constructor(
        activity: Activity,
        store: BrowserStore,
        customTabId: String? = null,
        fragmentManager: FragmentManager,
        shareDelegate: ShareDelegate = DefaultShareDelegate(),
        loginValidationDelegate: LoginValidationDelegate? = null,
        isSaveLoginEnabled: () -> Boolean = { false },
        loginExceptionStorage: LoginExceptions? = null,
        loginPickerView: LoginPickerView? = null,
        onManageLogins: () -> Unit = {},
        onNeedToRequestPermissions: OnNeedToRequestPermissions
    ) : this(
        container = PromptContainer.Activity(activity),
        store = store,
        customTabId = customTabId,
        fragmentManager = fragmentManager,
        shareDelegate = shareDelegate,
        loginValidationDelegate = loginValidationDelegate,
        isSaveLoginEnabled = isSaveLoginEnabled,
        loginExceptionStorage = loginExceptionStorage,
        onNeedToRequestPermissions = onNeedToRequestPermissions,
        loginPickerView = loginPickerView,
        onManageLogins = onManageLogins
    )

    constructor(
        fragment: Fragment,
        store: BrowserStore,
        customTabId: String? = null,
        fragmentManager: FragmentManager,
        shareDelegate: ShareDelegate = DefaultShareDelegate(),
        loginValidationDelegate: LoginValidationDelegate? = null,
        isSaveLoginEnabled: () -> Boolean = { false },
        loginExceptionStorage: LoginExceptions? = null,
        loginPickerView: LoginPickerView? = null,
        onManageLogins: () -> Unit = {},
        onNeedToRequestPermissions: OnNeedToRequestPermissions
    ) : this(
        container = PromptContainer.Fragment(fragment),
        store = store,
        customTabId = customTabId,
        fragmentManager = fragmentManager,
        shareDelegate = shareDelegate,
        loginValidationDelegate = loginValidationDelegate,
        isSaveLoginEnabled = isSaveLoginEnabled,
        loginExceptionStorage = loginExceptionStorage,
        onNeedToRequestPermissions = onNeedToRequestPermissions,
        loginPickerView = loginPickerView,
        onManageLogins = onManageLogins
    )

    @Deprecated("Pass only activity or fragment instead")
    constructor(
        activity: Activity? = null,
        fragment: Fragment? = null,
        store: BrowserStore,
        customTabId: String? = null,
        fragmentManager: FragmentManager,
        loginPickerView: LoginPickerView? = null,
        onManageLogins: () -> Unit = {},
        onNeedToRequestPermissions: OnNeedToRequestPermissions
    ) : this(
        container = activity?.let { PromptContainer.Activity(it) }
            ?: fragment?.let { PromptContainer.Fragment(it) }
            ?: throw IllegalStateException(
                "activity and fragment references " +
                        "must not be both null, at least one must be initialized."
            ),
        store = store,
        customTabId = customTabId,
        fragmentManager = fragmentManager,
        shareDelegate = DefaultShareDelegate(),
        loginValidationDelegate = null,
        onNeedToRequestPermissions = onNeedToRequestPermissions,
        loginPickerView = loginPickerView,
        onManageLogins = onManageLogins
    )

    private val filePicker = FilePicker(container, store, customTabId, onNeedToRequestPermissions)

    private val loginPicker =
        loginPickerView?.let { LoginPicker(store, it, onManageLogins, customTabId) }

    override val onNeedToRequestPermissions
        get() = filePicker.onNeedToRequestPermissions

    /**
     * Starts observing the selected session to listen for prompt requests
     * and displays a dialog when needed.
     */
    override fun start() {
        promptAbuserDetector.resetJSAlertAbuseState()

        handlePromptScope = store.flowScoped { flow ->
            flow.map { state -> state.findTabOrCustomTabOrSelectedTab(customTabId) }
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

        // Dismiss all prompts when page loads are nearly finished. This prevents prompts from the
        // previous page from covering content. See Fenix#5326
        dismissPromptScope = store.flowScoped { flow ->
            flow.mapNotNull { state -> state.findTabOrCustomTabOrSelectedTab(customTabId) }
                .ifChanged { it.content.progress }
                .filter { it.content.progress >= PROGRESS_ALMOST_COMPLETE }
                .collect {
                    val prompt = activePrompt?.get()
                    if (prompt?.shouldDismissOnLoad() == true) {
                        prompt.dismiss()
                    }
                    activePrompt?.clear()
                    loginPicker?.dismissCurrentLoginSelect()
                }
        }

        // Dismiss prompts when a new tab is selected.
        sessionPromptScope = store.flowScoped { flow ->
            flow.ifChanged { browserState -> browserState.selectedTabId }
                .collect {
                    val prompt = activePrompt?.get()
                    if (prompt?.shouldDismissOnLoad() == true) {
                        prompt.dismiss()
                    }
                    activePrompt?.clear()
                    loginPicker?.dismissCurrentLoginSelect()
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
        handlePromptScope?.cancel()
        dismissPromptScope?.cancel()
        sessionPromptScope?.cancel()
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
                is Share -> handleShareRequest(promptRequest, session)
                is SelectLoginPrompt -> {
                    if (promptRequest.logins.isNotEmpty()) {
                        loginPicker?.handleSelectLoginRequest(
                            promptRequest
                        )
                    }
                }
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
    override fun onCancel(sessionId: String) {
        store.consumePromptFrom(sessionId, activePrompt) {
            when (it) {
                is BeforeUnload -> it.onStay()
                is Dismissible -> it.onDismiss()
                is Popup -> it.onDeny()
            }
        }
    }

    /**
     * Invoked when the user confirms the action on the dialog. This consumes
     * the [PromptFeature] value from the [SessionState] indicated by [sessionId].
     *
     * @param sessionId that requested to show the dialog.
     * @param value an optional value provided by the dialog as a result of confirming the action.
     */
    @Suppress("UNCHECKED_CAST", "ComplexMethod")
    override fun onConfirm(sessionId: String, value: Any?) {
        store.consumePromptFrom(sessionId, activePrompt) {
            try {
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
                    is BeforeUnload -> it.onLeave()
                    is Popup -> it.onAllow()
                    is MultipleChoice -> it.onConfirm(value as Array<Choice>)

                    is Authentication -> {
                        val (user, password) = value as Pair<String, String>
                        it.onConfirm(user, password)
                    }

                    is TextPrompt -> {
                        val (shouldNotShowMoreDialogs, text) = value as Pair<Boolean, String>

                        promptAbuserDetector.userWantsMoreDialogs(!shouldNotShowMoreDialogs)
                        it.onConfirm(!shouldNotShowMoreDialogs, text)
                    }

                    is Share -> it.onSuccess()

                    is SaveLoginPrompt -> it.onConfirm(value as Login)

                    is Confirm -> {
                        val (isCheckBoxChecked, buttonType) =
                            value as Pair<Boolean, MultiButtonDialogFragment.ButtonType>
                        promptAbuserDetector.userWantsMoreDialogs(!isCheckBoxChecked)
                        when (buttonType) {
                            MultiButtonDialogFragment.ButtonType.POSITIVE ->
                                it.onConfirmPositiveButton(!isCheckBoxChecked)
                            MultiButtonDialogFragment.ButtonType.NEGATIVE ->
                                it.onConfirmNegativeButton(!isCheckBoxChecked)
                            MultiButtonDialogFragment.ButtonType.NEUTRAL ->
                                it.onConfirmNeutralButton(!isCheckBoxChecked)
                        }
                    }
                }
            } catch (e: ClassCastException) {
                throw IllegalArgumentException(
                    "PromptFeature onConsume cast failed with ${it.javaClass}",
                    e
                )
            }
        }
    }

    /**
     * Invoked when the user is requesting to clear the selected value from the dialog.
     * This consumes the [PromptFeature] value from the [SessionState] indicated by [sessionId].
     *
     * @param sessionId that requested to show the dialog.
     */
    override fun onClear(sessionId: String) {
        store.consumePromptFrom(sessionId, activePrompt) {
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

    private fun handleShareRequest(promptRequest: Share, session: SessionState) {
        shareDelegate.showShareSheet(
            context = container.context,
            shareData = promptRequest.data,
            onDismiss = { onCancel(session.id) },
            onSuccess = { onConfirm(session.id, null) }
        )
    }

    @Suppress("ComplexMethod", "LongMethod")
    @VisibleForTesting(otherwise = PRIVATE)
    internal fun handleDialogsRequest(
        promptRequest: PromptRequest,
        session: SessionState
    ) {
        // Requests that are handled with dialogs
        val dialog = when (promptRequest) {

            is SaveLoginPrompt -> {
                if (!isSaveLoginEnabled.invoke()) return

                if (loginValidationDelegate == null) {
                    logger.debug(
                        "Ignoring received SaveLoginPrompt because PromptFeature." +
                                "loginValidationDelegate is null. If you are trying to autofill logins, " +
                                "try attaching a LoginValidationDelegate to PromptFeature"
                    )
                    return
                }

                SaveLoginDialogFragment.newInstance(
                    sessionId = session.id,
                    hint = promptRequest.hint,
                    // For v1, we only handle a single login and drop all others on the floor
                    login = promptRequest.logins[0]
                )
            }

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
                        onlyShowPassword,
                        session.content.url
                    )
                }
            }

            is Color -> ColorPickerDialogFragment.newInstance(
                session.id,
                promptRequest.defaultColor
            )

            is Popup -> {

                val title = container.getString(R.string.mozac_feature_prompts_popup_dialog_title)
                val positiveLabel = container.getString(R.string.mozac_feature_prompts_allow)
                val negativeLabel = container.getString(R.string.mozac_feature_prompts_deny)

                ConfirmDialogFragment.newInstance(
                    sessionId = session.id,
                    title = title,
                    message = promptRequest.targetUri,
                    positiveButtonText = positiveLabel,
                    negativeButtonText = negativeLabel
                )
            }
            is BeforeUnload -> {

                val title =
                    container.getString(R.string.mozac_feature_prompt_before_unload_dialog_title)
                val body =
                    container.getString(R.string.mozac_feature_prompt_before_unload_dialog_body)
                val leaveLabel =
                    container.getString(R.string.mozac_feature_prompts_before_unload_leave)
                val stayLabel =
                    container.getString(R.string.mozac_feature_prompts_before_unload_stay)

                ConfirmDialogFragment.newInstance(
                    sessionId = session.id,
                    title = title,
                    message = body,
                    positiveButtonText = leaveLabel,
                    negativeButtonText = stayLabel
                )
            }

            is Confirm -> {
                with(promptRequest) {
                    val positiveButton = if (positiveButtonTitle.isEmpty()) {
                        container.getString(R.string.mozac_feature_prompts_ok)
                    } else {
                        positiveButtonTitle
                    }
                    val negativeButton = if (positiveButtonTitle.isEmpty()) {
                        container.getString(R.string.mozac_feature_prompts_cancel)
                    } else {
                        positiveButtonTitle
                    }

                    MultiButtonDialogFragment.newInstance(
                        session.id,
                        title,
                        message,
                        promptAbuserDetector.areDialogsBeingAbused(),
                        false,
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
            activePrompt = WeakReference(dialog)
        } else {
            (promptRequest as Dismissible).onDismiss()
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
            is BeforeUnload,
            is Popup,
            is SaveLoginPrompt,
            is SelectLoginPrompt,
            is Share -> true
            is Alert, is TextPrompt, is Confirm -> promptAbuserDetector.shouldShowMoreDialogs
        }
    }
}

internal fun BrowserStore.consumePromptFrom(
    sessionId: String?,
    activePrompt: WeakReference<PromptDialogFragment>? = null,
    consume: (PromptRequest) -> Unit
) {
    if (sessionId == null) {
        state.selectedTab
    } else {
        state.findTabOrCustomTabOrSelectedTab(sessionId)
    }?.let { tab ->
        activePrompt?.clear()
        tab.content.promptRequest?.let {
            consume(it)
            dispatch(ContentAction.ConsumePromptRequestAction(tab.id))
        }
    }
}
