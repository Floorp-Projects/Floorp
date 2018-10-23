/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.toolbar

import android.support.annotation.DrawableRes
import android.support.v7.widget.AppCompatImageButton
import android.support.v7.widget.AppCompatImageView
import android.util.TypedValue
import android.view.View
import android.view.ViewGroup
import android.widget.ImageButton
import java.lang.ref.WeakReference

/**
 * Interface to be implemented by components that provide browser toolbar functionality.
 */
interface Toolbar {
    /**
     * Sets/Gets the URL to be displayed on the toolbar.
     */
    var url: String

    /**
     * Displays the currently used search terms as part of this Toolbar.
     *
     * @param searchTerms the search terms used by the current session
     */
    fun setSearchTerms(searchTerms: String)

    /**
     * Displays the given loading progress. Expects values in the range [0, 100].
     */
    fun displayProgress(progress: Int)

    /**
     * Should be called by an activity when the user pressed the back key of the device.
     *
     * @return Returns true if the back press event was handled and should not be propagated further.
     */
    fun onBackPressed(): Boolean

    /**
     * Registers the given function to be invoked when the user selected a new URL i.e. is done
     * editing.
     *
     * @param listener the listener function
     */
    fun setOnUrlCommitListener(listener: (String) -> Unit)

    /**
     * Adds an action to be displayed on the right side of the toolbar in display mode.
     *
     * Related:
     * https://developer.mozilla.org/en-US/Add-ons/WebExtensions/user_interface/Browser_action
     */
    fun addBrowserAction(action: Action)

    /**
     * Adds an action to be displayed on the right side of the URL in display mode.
     *
     * Related:
     * https://developer.mozilla.org/en-US/Add-ons/WebExtensions/user_interface/Page_actions
     */
    fun addPageAction(action: Action)

    /**
     * Adds an action to be displayed on the far left side of the URL in display mode.
     */
    fun addNavigationAction(action: Action)

    /**
     * Casts this toolbar to an Android View object.
     */
    fun asView(): View = this as View

    /**
     * Registers the given listener to be invoked when the user edits the URL.
     */
    fun setOnEditListener(listener: OnEditListener)

    /**
     * Listener to be invoked when the user edits the URL.
     */
    interface OnEditListener {
        /**
         * Fired when the toolbar switches to edit mode.
         */
        fun onStartEditing() = Unit

        /**
         * Fired when the toolbar switches back to display mode.
         */
        fun onStopEditing() = Unit

        /**
         * Fired whenever the user changes the text in the address bar.
         */
        fun onTextChanged(text: String) = Unit
    }

    /**
     * Generic interface for actions to be added to the toolbar.
     */
    interface Action {
        val visible: () -> Boolean
            get() = { true }

        fun createView(parent: ViewGroup): View

        fun bind(view: View)
    }

    /**
     * An action button to be added to the toolbar.
     *
     * @param imageResource The drawable to be shown.
     * @param contentDescription The content description to use.
     * @param visible Lambda that returns true or false to indicate whether this button should be shown.
     * @param background A custom (stateful) background drawable resource to be used.
     * @param listener Callback that will be invoked whenever the button is pressed
     */
    open class ActionButton(
        @DrawableRes private val imageResource: Int,
        private val contentDescription: String,
        override val visible: () -> Boolean = { true },
        @DrawableRes private val background: Int? = null,
        private val listener: () -> Unit
    ) : Action {
        override fun createView(parent: ViewGroup): View = AppCompatImageButton(parent.context).also {
            it.setImageResource(imageResource)
            it.contentDescription = contentDescription
            it.setOnClickListener { listener.invoke() }

            if (background == null) {
                val outValue = TypedValue()
                parent.context.theme.resolveAttribute(
                        android.R.attr.selectableItemBackgroundBorderless,
                        outValue,
                        true)
                it.setBackgroundResource(outValue.resourceId)
            } else {
                it.setBackgroundResource(background)
            }
        }

        override fun bind(view: View) = Unit
    }

    /**
     * An action button with two states, selected and unselected. When the button is pressed, the
     * state changes automatically.
     *
     * @param imageResource The drawable to be shown if the button is in unselected state.
     * @param imageResourceSelected The drawable to be shown if the button is in selected state.
     * @param contentDescription The content description to use if the button is in unselected state.
     * @param contentDescriptionSelected The content description to use if the button is in selected state.
     * @param visible Lambda that returns true or false to indicate whether this button should be shown.
     * @param selected Sets whether this button should be selected initially.
     * @param background A custom (stateful) background drawable resource to be used.
     * @param listener Callback that will be invoked whenever the checked state changes.
     */
    open class ActionToggleButton(
        @DrawableRes private val imageResource: Int,
        @DrawableRes private val imageResourceSelected: Int,
        private val contentDescription: String,
        private val contentDescriptionSelected: String,
        override val visible: () -> Boolean = { true },
        private var selected: Boolean = false,
        @DrawableRes private val background: Int? = null,
        private val listener: (Boolean) -> Unit
    ) : Action {
        private var view: WeakReference<ImageButton>? = null

        override fun createView(parent: ViewGroup): View = AppCompatImageButton(parent.context).also {
            view = WeakReference(it)

            it.setOnClickListener { toggle() }
            it.isSelected = selected

            updateViewState()

            if (background == null) {
                val outValue = TypedValue()
                parent.context.theme.resolveAttribute(
                        android.R.attr.selectableItemBackgroundBorderless,
                        outValue,
                        true)

                it.setBackgroundResource(outValue.resourceId)
            } else {
                it.setBackgroundResource(background)
            }
        }

        /**
         * Changes the selected state of the action to the inverse of its current state.
         *
         * @param notifyListener If true (default) the listener will be notified about the state change.
         */
        fun toggle(notifyListener: Boolean = true) {
            setSelected(!selected, notifyListener)
        }

        /**
         * Changes the selected state of the action.
         *
         * @param selected The new selected state
         * @param notifyListener If true (default) the listener will be notified about a state change.
         */
        fun setSelected(selected: Boolean, notifyListener: Boolean = true) {
            if (this.selected == selected) {
                // Nothing to do here.
                return
            }

            this.selected = selected
            updateViewState()

            if (notifyListener) {
                listener.invoke(selected)
            }
        }

        /**
         * Returns the current selected state of the action.
         */
        fun isSelected() = selected

        private fun updateViewState() {
            view?.get()?.let {
                it.isSelected = selected

                if (selected) {
                    it.setImageResource(imageResourceSelected)
                    it.contentDescription = contentDescriptionSelected
                } else {
                    it.setImageResource(imageResource)
                    it.contentDescription = contentDescription
                }
            }
        }

        override fun bind(view: View) = Unit
    }

    /**
     * An "empty" action with a desired width to be used as "placeholder".
     *
     * @param desiredWidth The desired width in density independent pixels for this action.
     */
    open class ActionSpace(
        private val desiredWidth: Int
    ) : Action {
        override fun createView(parent: ViewGroup): View = View(parent.context).apply {
            minimumWidth = desiredWidth
        }

        override fun bind(view: View) = Unit
    }

    /**
     * An action that just shows a static, non-clickable image.
     *
     * @param imageResource The drawable to be shown.
     * @param contentDescription Optional content description to be used. If no content description
     *                           is provided then this view will be treated as not important for
     *                           accessibility.
     */
    open class ActionImage(
        @DrawableRes private val imageResource: Int,
        private val contentDescription: String? = null
    ) : Action {
        override fun createView(parent: ViewGroup): View = AppCompatImageView(parent.context).also {
            val drawable = parent.context.resources.getDrawable(imageResource, parent.context.theme)
            it.minimumWidth = drawable.intrinsicWidth

            it.setImageDrawable(drawable)

            it.contentDescription = contentDescription
            it.importantForAccessibility = if (contentDescription.isNullOrEmpty()) {
                View.IMPORTANT_FOR_ACCESSIBILITY_NO
            } else {
                View.IMPORTANT_FOR_ACCESSIBILITY_AUTO
            }
        }

        override fun bind(view: View) = Unit
    }
}
