/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose.cfr

import android.view.View
import androidx.annotation.VisibleForTesting
import androidx.compose.runtime.Composable
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.compose.cfr.CFRPopup.PopupAlignment
import java.lang.ref.WeakReference

/**
 * Properties used to customize the behavior of a [CFRPopup].
 *
 * @property popupWidth Width of the popup. Defaults to [CFRPopup.DEFAULT_WIDTH].
 * @property popupAlignment Where in relation to it's anchor should the popup be placed.
 * @property indicatorDirection The direction the indicator arrow is pointing.
 * @property dismissOnBackPress Whether the popup can be dismissed by pressing the back button.
 * If true, pressing the back button will also call onDismiss().
 * @property dismissOnClickOutside Whether the popup can be dismissed by clicking outside the
 * popup's bounds. If true, clicking outside the popup will call onDismiss().
 * @property overlapAnchor How the popup's indicator will be shown in relation to the anchor:
 *   - true - indicator will be shown exactly in the middle horizontally and vertically
 *   - false - indicator will be shown horizontally in the middle of the anchor but immediately below or above it
 * @property popupVerticalOffset Vertical distance between the indicator arrow and the anchor.
 * This only applies if [overlapAnchor] is `false`.
 * @property indicatorArrowStartOffset Maximum distance between the popup start and the indicator arrow.
 * If there isn't enough space this could automatically be overridden up to 0 such that
 * the indicator arrow will be pointing to the middle of the anchor.
 */
data class CFRPopupProperties(
    val popupWidth: Dp = CFRPopup.DEFAULT_WIDTH.dp,
    val popupAlignment: PopupAlignment = PopupAlignment.BODY_TO_ANCHOR_CENTER,
    val indicatorDirection: CFRPopup.IndicatorDirection = CFRPopup.IndicatorDirection.UP,
    val dismissOnBackPress: Boolean = true,
    val dismissOnClickOutside: Boolean = true,
    val overlapAnchor: Boolean = false,
    val popupVerticalOffset: Dp = CFRPopup.DEFAULT_VERTICAL_OFFSET.dp,
    val indicatorArrowStartOffset: Dp = CFRPopup.DEFAULT_INDICATOR_START_OFFSET.dp,
)

/**
 * CFR - Contextual Feature Recommendation popup.
 *
 * @param text [String] shown as the popup content.
 * @param anchor [View] that will serve as the anchor of the popup and serve as lifecycle owner
 * for this popup also.
 * @param properties [CFRPopupProperties] allowing to customize the popup behavior.
 * @param onDismiss Callback for when the popup is dismissed indicating also if the dismissal
 * was explicit - by tapping the "X" button or not.
 * @param action Optional other composable to show just below the popup text.
 */
class CFRPopup(
    @get:VisibleForTesting internal val text: String,
    @get:VisibleForTesting internal val anchor: View,
    @get:VisibleForTesting internal val properties: CFRPopupProperties = CFRPopupProperties(),
    @get:VisibleForTesting internal val onDismiss: (Boolean) -> Unit = {},
    @get:VisibleForTesting internal val action: @Composable (() -> Unit) = {},
) {
    // This is just a facade for the CFRPopupFullScreenLayout composable offering a cleaner API.

    @VisibleForTesting
    internal var popup: WeakReference<CFRPopupFullscreenLayout>? = null

    /**
     * Construct and display a styled CFR popup shown at the coordinates of [anchor].
     * This popup will be dismissed when the user clicks on the "x" button or based on other user actions
     * with such behavior set in [CFRPopupProperties].
     */
    fun show() {
        anchor.post {
            // When we're in this Runnable, the 'show' method might have been called right before
            // the activity is no longer attached to the WindowManager. When we get to calling
            // the CFRPopupFullscreenLayout#show method below, we are now trying to attach the View
            // with the WindowManager that has an unusable Activity.
            //
            // To protect against this, within this same Runnable, we check if the anchor view is
            // safe to use before continuing.
            //
            // See: https://bugzilla.mozilla.org/show_bug.cgi?id=1799996
            if (anchor.context == null || !anchor.isAttachedToWindow) {
                return@post
            }

            CFRPopupFullscreenLayout(text, anchor, properties, onDismiss, action).apply {
                this.show()
                popup = WeakReference(this)
            }
        }
    }

    /**
     * Immediately dismiss this CFR popup.
     * The [onDismiss] callback won't be fired.
     */
    fun dismiss() {
        popup?.get()?.dismiss()
    }

    /**
     * Possible direction for the arrow indicator of a CFR popup.
     * The direction is expressed in relation with the popup body containing the text.
     */
    enum class IndicatorDirection {
        UP,
        DOWN,
    }

    /**
     * Possible alignments of the popup in relation to it's anchor.
     */
    enum class PopupAlignment {
        /**
         * The popup body will be centered in the space occupied by the anchor.
         * Recommended to be used when the anchor is wider than the popup.
         */
        BODY_TO_ANCHOR_CENTER,

        /**
         * The popup body will be shown aligned to exactly the anchor start.
         */
        BODY_TO_ANCHOR_START,

        /**
         * The popup will be aligned such that the indicator arrow will point to exactly the middle of the anchor.
         * Recommended to be used when there are multiple widgets displayed horizontally so that this will allow
         * to indicate exactly which widget the popup refers to.
         */
        INDICATOR_CENTERED_IN_ANCHOR,
    }

    companion object {
        /**
         * Default width for all CFRs.
         */
        internal const val DEFAULT_WIDTH = 335

        /**
         * Fixed horizontal padding.
         * Allows the close button to extend with 10dp more to the end and intercept touches to
         * a bit outside of the popup to ensure it respects a11y recommendations of 48dp size while
         * also offer a bit more space to the text.
         */
        internal const val DEFAULT_HORIZONTAL_PADDING = 10

        /**
         * How tall the indicator arrow should be.
         * This will also affect the width of the indicator's base which is double the height value.
         */
        internal const val DEFAULT_INDICATOR_HEIGHT = 7

        /**
         * Maximum distance between the popup start and the indicator.
         */
        internal const val DEFAULT_INDICATOR_START_OFFSET = 30

        /**
         * Corner radius for the popup body.
         */
        internal const val DEFAULT_CORNER_RADIUS = 12

        /**
         * Vertical distance between the indicator arrow and the anchor.
         */
        internal const val DEFAULT_VERTICAL_OFFSET = 9
    }
}
