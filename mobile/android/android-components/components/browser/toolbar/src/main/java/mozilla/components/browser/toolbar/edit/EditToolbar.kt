/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.edit

import android.content.Context
import android.graphics.Typeface
import android.graphics.drawable.Drawable
import android.view.KeyEvent
import android.view.View
import android.widget.ImageView
import androidx.annotation.ColorInt
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
     * @property cancel Color tint used for the "cancel" icon to leave "edit mode".
     * @property icon Color tint of the icon displayed in front of the URL.
     * @property hint Text color of the hint shown when the URL field is empty.
     * @property text Text color of the URL.
     * @property suggestionBackground The background color used for autocomplete suggestions.
     * @property suggestionForeground The foreground color used for autocomplete suggestions.
     */
    data class Colors(
        @ColorInt val cancel: Int,
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

    private val views = object {
        val backgroundView = rootView.findViewById<ImageView>(R.id.mozac_browser_toolbar_background)
        val iconView = rootView.findViewById<ImageView>(R.id.mozac_browser_toolbar_edit_icon)
        val editActions = rootView.findViewById<ActionContainer>(R.id.mozac_browser_toolbar_edit_actions)
        val cancelView = rootView.findViewById<ImageView>(R.id.mozac_browser_toolbar_cancel_view).apply {
            setOnClickListener {
                // We set text to an empty string instead of using clear to avoid #3612.
                urlView.setText("")
            }
        }
        val urlView: InlineAutocompleteEditText = rootView.findViewById<InlineAutocompleteEditText>(
            R.id.mozac_browser_toolbar_edit_url_view
        ).apply {
            setOnCommitListener {
                // We emit the fact before notifying the listener because otherwise the listener may cause a focus
                // change which may reset the autocomplete state that we want to report here.
                emitCommitFact(autocompleteResult)

                toolbar.onUrlEntered(text.toString())
            }

            setOnTextChangeListener { text, _ ->
                cancelView.isVisible = text.isNotBlank()
                editListener?.onTextChanged(text)
            }

            setOnDispatchKeyEventPreImeListener { event ->
                if (event?.keyCode == KeyEvent.KEYCODE_BACK && editListener?.onCancelEditing() != false) {
                    toolbar.displayMode()
                }
                false
            }
        }
    }

    /**
     * Customizable colors in "edit mode".
     */
    var colors: Colors = Colors(
        cancel = ContextCompat.getColor(context, R.color.photonWhite),
        icon = null,
        hint = views.urlView.currentHintTextColor,
        text = views.urlView.currentTextColor,
        suggestionBackground = views.urlView.autoCompleteBackgroundColor,
        suggestionForeground = views.urlView.autoCompleteForegroundColor
    )
    set(value) {
        field = value

        views.cancelView.setColorFilter(value.cancel)

        if (value.icon != null) {
            views.iconView.setColorFilter(value.icon)
        }

        views.urlView.setHintTextColor(value.hint)
        views.urlView.setTextColor(value.text)
        views.urlView.autoCompleteBackgroundColor = value.suggestionBackground
        views.urlView.autoCompleteForegroundColor = value.suggestionForeground
    }

    /**
     * Sets the background that will be drawn behind the URL, icon and edit actions.
     */
    fun setUrlBackground(background: Drawable?) {
        views.backgroundView.setImageDrawable(background)
    }

    /**
     * Sets an icon that will be drawn in front of the URL.
     */
    fun setIcon(icon: Drawable, contentDescription: String) {
        views.iconView.setImageDrawable(icon)
        views.iconView.contentDescription = contentDescription
    }

    /**
     * Sets the text to be displayed when the URL of the toolbar is empty.
     */
    var hint: String
        get() = views.urlView.hint.toString()
        set(value) { views.urlView.hint = value }

    /**
     * Sets the size of the text for the URL/search term displayed in the toolbar.
     */
    var textSize: Float
        get() = views.urlView.textSize
        set(value) {
            views.urlView.textSize = value
        }

    /**
     * Sets the typeface of the text for the URL/search term displayed in the toolbar.
     */
    var typeface: Typeface
        get() = views.urlView.typeface
        set(value) { views.urlView.typeface = value }

    /**
     * Sets a listener to be invoked when focus of the URL input view (in edit mode) changed.
     */
    internal fun setOnEditFocusChangeListener(listener: (Boolean) -> Unit) {
        views.urlView.onFocusChangeListener = View.OnFocusChangeListener { _, hasFocus ->
            listener.invoke(hasFocus)
        }
    }

    /**
     * Focuses the url input field.
     */
    fun focus() {
        views.urlView.requestFocus()
    }

    internal fun stopEditing() {
        editListener?.onStopEditing()
    }

    internal fun startEditing() {
        editListener?.onStartEditing()
    }

    internal var editListener: Toolbar.OnEditListener? = null

    internal fun setAutocompleteListener(filter: suspend (String, AutocompleteDelegate) -> Unit) {
        views.urlView.setOnFilterListener(
            AsyncFilterListener(views.urlView, autocompleteDispatcher, filter)
        )
    }

    internal fun invalidateActions() {
        views.editActions.invalidateActions()
    }

    internal fun addEditAction(action: Toolbar.Action) {
        views.editActions.addAction(action)
    }

    internal fun updateUrl(url: String, shouldAutoComplete: Boolean = false) {
        views.urlView.setText(url, shouldAutoComplete)
    }

    /**
     * Select the entire text in the URL input field.
     */
    internal fun selectAll() {
        views.urlView.selectAll()
    }

    /**
     * Sets/gets private mode.
     *
     * In private mode the IME should not update any personalized data such as typing history and personalized language
     * model based on what the user typed.
     */
    internal var private: Boolean
        get() = (views.urlView.imeOptions and EditorInfoCompat.IME_FLAG_NO_PERSONALIZED_LEARNING) != 0
        set(value) {
            views.urlView.imeOptions = if (value) {
                views.urlView.imeOptions or EditorInfoCompat.IME_FLAG_NO_PERSONALIZED_LEARNING
            } else {
                views.urlView.imeOptions and (EditorInfoCompat.IME_FLAG_NO_PERSONALIZED_LEARNING.inv())
            }
        }
}
