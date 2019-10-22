/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.edit

import android.content.Context
import android.graphics.Typeface
import android.graphics.drawable.Drawable
import android.view.KeyEvent
import android.view.View
import android.view.inputmethod.InputMethodManager
import android.widget.ImageView
import androidx.annotation.ColorInt
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.PRIVATE
import androidx.core.content.ContextCompat
import androidx.core.view.inputmethod.EditorInfoCompat
import androidx.core.view.isVisible
import kotlinx.coroutines.CoroutineExceptionHandler
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.asCoroutineDispatcher
import mozilla.components.browser.toolbar.AsyncFilterListener
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.browser.toolbar.R
import mozilla.components.browser.toolbar.facts.emitCommitFact
import mozilla.components.browser.toolbar.internal.ActionContainer
import mozilla.components.concept.toolbar.AutocompleteDelegate
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.android.view.showKeyboard
import mozilla.components.ui.autocomplete.InlineAutocompleteEditText
import java.util.concurrent.Executors

private const val AUTOCOMPLETE_QUERY_THREADS = 3

/**
 * Sub-component of the browser toolbar responsible for allowing the user to edit the URL ("edit mode").
 *
 * Structure:
 * +------+---------------------------+---------+------+
 * | icon |           url             | actions | exit |
 * +------+---------------------------+---------+------+
 *
 * - icon: Optional icon that will be shown in front of the URL.
 * - url: Editable URL of the currently displayed website
 * - actions: Optional action icons injected by other components (e.g. barcode scanner)
 * - exit: Button that switches back to display mode or invoke an app-defined callback.
 */
@Suppress("TooManyFunctions")
class EditToolbar internal constructor(
    context: Context,
    private val toolbar: BrowserToolbar,
    internal val rootView: View
) {
    private val logger = Logger("EditToolbar")

    /**
     * Data class holding the customizable colors in "edit mode".
     *
     * @property clear Color tint used for the "cancel" icon to leave "edit mode".
     * @property icon Color tint of the icon displayed in front of the URL.
     * @property hint Text color of the hint shown when the URL field is empty.
     * @property text Text color of the URL.
     * @property suggestionBackground The background color used for autocomplete suggestions.
     * @property suggestionForeground The foreground color used for autocomplete suggestions.
     */
    data class Colors(
        @ColorInt val clear: Int,
        @ColorInt val icon: Int?,
        @ColorInt val hint: Int,
        @ColorInt val text: Int,
        @ColorInt val suggestionBackground: Int,
        @ColorInt val suggestionForeground: Int?
    )

    private val autocompleteDispatcher = SupervisorJob() +
        Executors.newFixedThreadPool(AUTOCOMPLETE_QUERY_THREADS).asCoroutineDispatcher() +
        CoroutineExceptionHandler { _, throwable ->
            logger.error("Error while processing autocomplete input", throwable)
        }

    @VisibleForTesting(otherwise = PRIVATE)
    internal val views = EditToolbarViews(
        background = rootView.findViewById<ImageView>(R.id.mozac_browser_toolbar_background),
        icon = rootView.findViewById<ImageView>(R.id.mozac_browser_toolbar_edit_icon),
        editActions = rootView.findViewById<ActionContainer>(R.id.mozac_browser_toolbar_edit_actions),
        clear = rootView.findViewById<ImageView>(R.id.mozac_browser_toolbar_clear_view).apply {
            setOnClickListener {
                onClear()
            }
        },
        url = rootView.findViewById<InlineAutocompleteEditText>(
            R.id.mozac_browser_toolbar_edit_url_view
        ).apply {
            setOnCommitListener {
                // We emit the fact before notifying the listener because otherwise the listener may cause a focus
                // change which may reset the autocomplete state that we want to report here.
                emitCommitFact(autocompleteResult)

                toolbar.onUrlEntered(text.toString())
            }

            setOnTextChangeListener { text, _ ->
                onTextChanged(text)
            }

            setOnDispatchKeyEventPreImeListener { event ->
                if (event?.keyCode == KeyEvent.KEYCODE_BACK && editListener?.onCancelEditing() != false) {
                    toolbar.displayMode()
                }
                false
            }
        }
    )

    /**
     * Customizable colors in "edit mode".
     */
    var colors: Colors = Colors(
        clear = ContextCompat.getColor(context, R.color.photonWhite),
        icon = null,
        hint = views.url.currentHintTextColor,
        text = views.url.currentTextColor,
        suggestionBackground = views.url.autoCompleteBackgroundColor,
        suggestionForeground = views.url.autoCompleteForegroundColor
    )
    set(value) {
        field = value

        views.clear.setColorFilter(value.clear)

        if (value.icon != null) {
            views.icon.setColorFilter(value.icon)
        }

        views.url.setHintTextColor(value.hint)
        views.url.setTextColor(value.text)
        views.url.autoCompleteBackgroundColor = value.suggestionBackground
        views.url.autoCompleteForegroundColor = value.suggestionForeground
    }

    /**
     * Sets the background that will be drawn behind the URL, icon and edit actions.
     */
    fun setUrlBackground(background: Drawable?) {
        views.background.setImageDrawable(background)
    }

    /**
     * Sets an icon that will be drawn in front of the URL.
     */
    fun setIcon(icon: Drawable, contentDescription: String) {
        views.icon.setImageDrawable(icon)
        views.icon.contentDescription = contentDescription
        views.icon.visibility = View.VISIBLE
    }

    /**
     * Sets the text to be displayed when the URL of the toolbar is empty.
     */
    var hint: String
        get() = views.url.hint.toString()
        set(value) { views.url.hint = value }

    /**
     * Sets the size of the text for the URL/search term displayed in the toolbar.
     */
    var textSize: Float
        get() = views.url.textSize
        set(value) {
            views.url.textSize = value
        }

    /**
     * Sets the typeface of the text for the URL/search term displayed in the toolbar.
     */
    var typeface: Typeface
        get() = views.url.typeface
        set(value) { views.url.typeface = value }

    /**
     * Sets a listener to be invoked when focus of the URL input view (in edit mode) changed.
     */
    internal fun setOnEditFocusChangeListener(listener: (Boolean) -> Unit) {
        views.url.onFocusChangeListener = View.OnFocusChangeListener { _, hasFocus ->
            listener.invoke(hasFocus)
        }
    }

    /**
     * Focuses the url input field and shows the virtual keyboard if needed.
     */
    fun focus() {
        views.url.showKeyboard(flags = InputMethodManager.SHOW_FORCED)
    }

    internal fun stopEditing() {
        editListener?.onStopEditing()
    }

    internal fun startEditing() {
        editListener?.onStartEditing()
    }

    internal var editListener: Toolbar.OnEditListener? = null

    internal fun setAutocompleteListener(filter: suspend (String, AutocompleteDelegate) -> Unit) {
        views.url.setOnFilterListener(
            AsyncFilterListener(views.url, autocompleteDispatcher, filter)
        )
    }

    internal fun invalidateActions() {
        views.editActions.invalidateActions()
    }

    internal fun addEditAction(action: Toolbar.Action) {
        views.editActions.addAction(action)
    }

    internal fun updateUrl(url: String, shouldAutoComplete: Boolean = false) {
        views.url.setText(url, shouldAutoComplete)
    }

    /**
     * Select the entire text in the URL input field.
     */
    internal fun selectAll() {
        views.url.selectAll()
    }

    /**
     * Sets/gets private mode.
     *
     * In private mode the IME should not update any personalized data such as typing history and personalized language
     * model based on what the user typed.
     */
    internal var private: Boolean
        get() = (views.url.imeOptions and EditorInfoCompat.IME_FLAG_NO_PERSONALIZED_LEARNING) != 0
        set(value) {
            views.url.imeOptions = if (value) {
                views.url.imeOptions or EditorInfoCompat.IME_FLAG_NO_PERSONALIZED_LEARNING
            } else {
                views.url.imeOptions and (EditorInfoCompat.IME_FLAG_NO_PERSONALIZED_LEARNING.inv())
            }
        }

    private fun onClear() {
        // We set text to an empty string instead of using clear to avoid #3612.
        views.url.setText("")
    }

    private fun onTextChanged(text: String) {
        views.clear.isVisible = text.isNotBlank()
        editListener?.onTextChanged(text)
    }
}

/**
 * Internal holder for view references.
 */
internal class EditToolbarViews(
    val background: ImageView,
    val icon: ImageView,
    val editActions: ActionContainer,
    val clear: ImageView,
    val url: InlineAutocompleteEditText
)
