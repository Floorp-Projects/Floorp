/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine

import android.view.MotionEvent
import androidx.annotation.VisibleForTesting

/**
 * Don't yet have a response from the browser about how the touch was handled.
 */
const val INPUT_HANDLING_UNKNOWN = -1

// The below top-level values are following the same from [org.mozilla.geckoview.PanZoomController]
/**
 * The content has no scrollable element.
 *
 * @see [InputResultDetail.isTouchUnhandled]
 */
const val INPUT_UNHANDLED = 0

/**
 * The touch event is consumed by the [EngineView]
 *
 * @see [InputResultDetail.isTouchHandledByBrowser]
 */
const val INPUT_HANDLED = 1

/**
 * The touch event is consumed by the website through it's own touch listeners.
 *
 * @see [InputResultDetail.isTouchHandledByWebsite]
 */
const val INPUT_HANDLED_CONTENT = 2

/**
 * The website content is not scrollable.
 */
@VisibleForTesting
internal const val SCROLL_DIRECTIONS_NONE = 0

/**
 * The website content can be scrolled to the top.
 *
 * @see [InputResultDetail.canScrollToTop]
 */
@VisibleForTesting
internal const val SCROLL_DIRECTIONS_TOP = 1 shl 0

/**
 * The website content can be scrolled to the right.
 *
 * @see [InputResultDetail.canScrollToRight]
 */
@VisibleForTesting
internal const val SCROLL_DIRECTIONS_RIGHT = 1 shl 1

/**
 * The website content can be scrolled to the bottom.
 *
 * @see [InputResultDetail.canScrollToBottom]
 */
@VisibleForTesting
internal const val SCROLL_DIRECTIONS_BOTTOM = 1 shl 2

/**
 * The website content can be scrolled to the left.
 *
 * @see [InputResultDetail.canScrollToLeft]
 */
@VisibleForTesting
internal const val SCROLL_DIRECTIONS_LEFT = 1 shl 3

/**
 * The website content cannot be overscrolled.
 */
@VisibleForTesting
internal const val OVERSCROLL_DIRECTIONS_NONE = 0

/**
 * The website content can be overscrolled horizontally.
 *
 * @see [InputResultDetail.canOverscrollRight]
 * @see [InputResultDetail.canOverscrollLeft]
 */
@VisibleForTesting
internal const val OVERSCROLL_DIRECTIONS_HORIZONTAL = 1 shl 0

/**
 * The website content can be overscrolled vertically.
 *
 * @see [InputResultDetail.canOverscrollTop]
 * @see [InputResultDetail.canOverscrollBottom]
 */
@VisibleForTesting
internal const val OVERSCROLL_DIRECTIONS_VERTICAL = 1 shl 1

/**
 * All data about how a touch will be handled by the browser.
 * - whether the event is used for panning/zooming by the browser / by the website or will be ignored.
 * - whether the event can scroll the page and in what direction.
 * - whether the event can overscroll the page and in what direction.
 *
 * @param inputResult Indicates who will use the current [MotionEvent].
 * Possible values: [[INPUT_HANDLING_UNKNOWN], [INPUT_UNHANDLED], [INPUT_HANDLED], [INPUT_HANDLED_CONTENT]].
 *
 * @param scrollDirections Bitwise ORed value of the directions the page can be scrolled to.
 * This is the same as GeckoView's [org.mozilla.geckoview.PanZoomController.ScrollableDirections].
 *
 * @param overscrollDirections Bitwise ORed value of the directions the page can be overscrolled to.
 * This is the same as GeckoView's [org.mozilla.geckoview.PanZoomController.OverscrollDirections].
 */
@Suppress("TooManyFunctions")
class InputResultDetail private constructor(
    val inputResult: Int = INPUT_HANDLING_UNKNOWN,
    val scrollDirections: Int = SCROLL_DIRECTIONS_NONE,
    val overscrollDirections: Int = OVERSCROLL_DIRECTIONS_NONE,
) {

    override fun equals(other: Any?): Boolean {
        return if (this !== other) {
            if (other is InputResultDetail) {
                return inputResult == other.inputResult &&
                    scrollDirections == other.scrollDirections &&
                    overscrollDirections == other.overscrollDirections
            } else {
                false
            }
        } else {
            true
        }
    }

    @Suppress("MagicNumber")
    override fun hashCode(): Int {
        var hash = inputResult.hashCode()
        hash += (scrollDirections.hashCode()) * 10
        hash += (overscrollDirections.hashCode()) * 100

        return hash
    }

    override fun toString(): String {
        return StringBuilder("InputResultDetail \$${hashCode()} (")
            .append("Input ${getInputResultHandledDescription()}. ")
            .append("Content ${getScrollDirectionsDescription()} and ${getOverscrollDirectionsDescription()}")
            .append(')')
            .toString()
    }

    /**
     * Create a new instance of [InputResultDetail] with the option of keep some of the current values.
     *
     * The provided new values will be filtered out if not recognized and could corrupt the current state.
     */
    fun copy(
        inputResult: Int? = this.inputResult,
        scrollDirections: Int? = this.scrollDirections,
        overscrollDirections: Int? = this.overscrollDirections,
    ): InputResultDetail {
        // Ensure this data will not get corrupted by users sending unknown arguments

        val newValidInputResult = if (inputResult in INPUT_UNHANDLED..INPUT_HANDLED_CONTENT) {
            inputResult
        } else {
            this.inputResult
        }
        val newValidScrollDirections = if (scrollDirections in
            SCROLL_DIRECTIONS_NONE..(SCROLL_DIRECTIONS_LEFT or (SCROLL_DIRECTIONS_LEFT - 1))
        ) {
            scrollDirections
        } else {
            this.scrollDirections
        }
        val newValidOverscrollDirections = if (overscrollDirections in
            OVERSCROLL_DIRECTIONS_NONE..(OVERSCROLL_DIRECTIONS_VERTICAL or (OVERSCROLL_DIRECTIONS_VERTICAL - 1))
        ) {
            overscrollDirections
        } else {
            this.overscrollDirections
        }

        // The range check automatically checks for null but doesn't yet have a contract to say so.
        // As such it it safe to use the not-null assertion operator.
        return InputResultDetail(newValidInputResult!!, newValidScrollDirections!!, newValidOverscrollDirections!!)
    }

    /**
     * The [EngineView] has not yet responded on how it handled the [MotionEvent].
     */
    fun isTouchHandlingUnknown() = inputResult == INPUT_HANDLING_UNKNOWN

    /**
     * The [EngineView] handled the last [MotionEvent] to pan or zoom the content.
     */
    fun isTouchHandledByBrowser() = inputResult == INPUT_HANDLED

    /**
     * The website handled the last [MotionEvent] through it's own touch listeners
     * and consumed it without the [EngineView] panning or zooming the website
     */
    fun isTouchHandledByWebsite() = inputResult == INPUT_HANDLED_CONTENT

    /**
     * Neither the [EngineView], nor the website will handle this [MotionEvent].
     *
     * This might happen on a website without touch listeners that is not bigger than the screen
     * or when the content has no scrollable element.
     */
    fun isTouchUnhandled() = inputResult == INPUT_UNHANDLED

    /**
     * Whether the width of the webpage exceeds the display and the webpage can be scrolled to left.
     */
    fun canScrollToLeft(): Boolean =
        inputResult == INPUT_HANDLED &&
            scrollDirections and SCROLL_DIRECTIONS_LEFT != 0

    /**
     * Whether the height of the webpage exceeds the display and the webpage can be scrolled to top.
     */
    fun canScrollToTop(): Boolean =
        inputResult == INPUT_HANDLED &&
            scrollDirections and SCROLL_DIRECTIONS_TOP != 0

    /**
     * Whether the width of the webpage exceeds the display and the webpage can be scrolled to right.
     */
    fun canScrollToRight(): Boolean =
        inputResult == INPUT_HANDLED &&
            scrollDirections and SCROLL_DIRECTIONS_RIGHT != 0

    /**
     * Whether the height of the webpage exceeds the display and the webpage can be scrolled to bottom.
     */
    fun canScrollToBottom(): Boolean =
        inputResult == INPUT_HANDLED &&
            scrollDirections and SCROLL_DIRECTIONS_BOTTOM != 0

    /**
     * Whether the webpage can be overscrolled to the left.
     *
     * @return `true` if the page is already scrolled to the left most part
     * and the touch event is not handled by the webpage.
     */
    fun canOverscrollLeft(): Boolean =
        inputResult != INPUT_HANDLED_CONTENT &&
            (scrollDirections and SCROLL_DIRECTIONS_LEFT == 0) &&
            (overscrollDirections and OVERSCROLL_DIRECTIONS_HORIZONTAL != 0)

    /**
     * Whether the webpage can be overscrolled to the top.
     *
     * @return `true` if the page is already scrolled to the top most part
     * and the touch event is not handled by the webpage.
     */
    fun canOverscrollTop(): Boolean =
        inputResult != INPUT_HANDLED_CONTENT &&
            (scrollDirections and SCROLL_DIRECTIONS_TOP == 0) &&
            (overscrollDirections and OVERSCROLL_DIRECTIONS_VERTICAL != 0)

    /**
     * Whether the webpage can be overscrolled to the right.
     *
     * @return `true` if the page is already scrolled to the right most part
     * and the touch event is not handled by the webpage.
     */
    fun canOverscrollRight(): Boolean =
        inputResult != INPUT_HANDLED_CONTENT &&
            (scrollDirections and SCROLL_DIRECTIONS_RIGHT == 0) &&
            (overscrollDirections and OVERSCROLL_DIRECTIONS_HORIZONTAL != 0)

    /**
     * Whether the webpage can be overscrolled to the bottom.
     *
     * @return `true` if the page is already scrolled to the bottom most part
     * and the touch event is not handled by the webpage.
     */
    fun canOverscrollBottom(): Boolean =
        inputResult != INPUT_HANDLED_CONTENT &&
            (scrollDirections and SCROLL_DIRECTIONS_BOTTOM == 0) &&
            (overscrollDirections and OVERSCROLL_DIRECTIONS_VERTICAL != 0)

    @VisibleForTesting
    internal fun getInputResultHandledDescription() = when (inputResult) {
        INPUT_HANDLING_UNKNOWN -> INPUT_UNKNOWN_HANDLING_DESCRIPTION
        INPUT_HANDLED -> INPUT_HANDLED_TOSTRING_DESCRIPTION
        INPUT_HANDLED_CONTENT -> INPUT_HANDLED_CONTENT_TOSTRING_DESCRIPTION
        else -> INPUT_UNHANDLED_TOSTRING_DESCRIPTION
    }

    @VisibleForTesting
    internal fun getScrollDirectionsDescription(): String {
        if (scrollDirections == SCROLL_DIRECTIONS_NONE) {
            return SCROLL_IMPOSSIBLE_TOSTRING_DESCRIPTION
        }

        val scrollDirections = StringBuilder()
            .append(if (canScrollToLeft()) "$SCROLL_LEFT_TOSTRING_DESCRIPTION$TOSTRING_SEPARATOR" else "")
            .append(if (canScrollToTop()) "$SCROLL_TOP_TOSTRING_DESCRIPTION$TOSTRING_SEPARATOR" else "")
            .append(if (canScrollToRight()) "$SCROLL_RIGHT_TOSTRING_DESCRIPTION$TOSTRING_SEPARATOR" else "")
            .append(if (canScrollToBottom()) SCROLL_BOTTOM_TOSTRING_DESCRIPTION else "")
            .removeSuffix(TOSTRING_SEPARATOR)
            .toString()

        return if (scrollDirections.trim().isEmpty()) {
            SCROLL_IMPOSSIBLE_TOSTRING_DESCRIPTION
        } else {
            SCROLL_TOSTRING_DESCRIPTION + scrollDirections
        }
    }

    @VisibleForTesting
    internal fun getOverscrollDirectionsDescription(): String {
        if (overscrollDirections == OVERSCROLL_DIRECTIONS_NONE) {
            return OVERSCROLL_IMPOSSIBLE_TOSTRING_DESCRIPTION
        }

        val overscrollDirections = StringBuilder()
            .append(if (canOverscrollLeft()) "$SCROLL_LEFT_TOSTRING_DESCRIPTION$TOSTRING_SEPARATOR" else "")
            .append(if (canOverscrollTop()) "$SCROLL_TOP_TOSTRING_DESCRIPTION$TOSTRING_SEPARATOR" else "")
            .append(if (canOverscrollRight()) "$SCROLL_RIGHT_TOSTRING_DESCRIPTION$TOSTRING_SEPARATOR" else "")
            .append(if (canOverscrollBottom()) SCROLL_BOTTOM_TOSTRING_DESCRIPTION else "")
            .removeSuffix(TOSTRING_SEPARATOR)
            .toString()

        return if (overscrollDirections.trim().isEmpty()) {
            OVERSCROLL_IMPOSSIBLE_TOSTRING_DESCRIPTION
        } else {
            OVERSCROLL_TOSTRING_DESCRIPTION + overscrollDirections
        }
    }

    companion object {
        /**
         * Create a new instance of [InputResultDetail].
         *
         * @param verticalOverscrollInitiallyEnabled optional parameter for enabling pull to refresh
         * in the  cases in which this class can be used before valid values being set and it helps more to have
         * overscroll vertically allowed and then stop depending on the values with which this class is updated
         * rather than start with a disabled overscroll functionality for the current gesture.
         */
        fun newInstance(verticalOverscrollInitiallyEnabled: Boolean = false) = InputResultDetail(
            overscrollDirections = if (verticalOverscrollInitiallyEnabled) {
                OVERSCROLL_DIRECTIONS_VERTICAL
            } else {
                OVERSCROLL_DIRECTIONS_NONE
            },
        )

        @VisibleForTesting internal const val TOSTRING_SEPARATOR = ", "

        @VisibleForTesting internal const val INPUT_UNKNOWN_HANDLING_DESCRIPTION = "with unknown handling"

        @VisibleForTesting internal const val INPUT_HANDLED_TOSTRING_DESCRIPTION = "handled by the browser"

        @VisibleForTesting internal const val INPUT_HANDLED_CONTENT_TOSTRING_DESCRIPTION = "handled by the website"

        @VisibleForTesting internal const val INPUT_UNHANDLED_TOSTRING_DESCRIPTION = "unhandled"

        @VisibleForTesting internal const val SCROLL_IMPOSSIBLE_TOSTRING_DESCRIPTION = "cannot be scrolled"

        @VisibleForTesting internal const val OVERSCROLL_IMPOSSIBLE_TOSTRING_DESCRIPTION = "cannot be overscrolled"

        @VisibleForTesting internal const val SCROLL_TOSTRING_DESCRIPTION = "can be scrolled to "

        @VisibleForTesting internal const val OVERSCROLL_TOSTRING_DESCRIPTION = "can be overscrolled to "

        @VisibleForTesting internal const val SCROLL_LEFT_TOSTRING_DESCRIPTION = "left"

        @VisibleForTesting internal const val SCROLL_TOP_TOSTRING_DESCRIPTION = "top"

        @VisibleForTesting internal const val SCROLL_RIGHT_TOSTRING_DESCRIPTION = "right"

        @VisibleForTesting internal const val SCROLL_BOTTOM_TOSTRING_DESCRIPTION = "bottom"
    }
}
