/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine

import mozilla.components.concept.engine.InputResultDetail.Companion.INPUT_HANDLED_CONTENT_TOSTRING_DESCRIPTION
import mozilla.components.concept.engine.InputResultDetail.Companion.INPUT_HANDLED_TOSTRING_DESCRIPTION
import mozilla.components.concept.engine.InputResultDetail.Companion.INPUT_UNHANDLED_TOSTRING_DESCRIPTION
import mozilla.components.concept.engine.InputResultDetail.Companion.INPUT_UNKNOWN_HANDLING_DESCRIPTION
import mozilla.components.concept.engine.InputResultDetail.Companion.OVERSCROLL_IMPOSSIBLE_TOSTRING_DESCRIPTION
import mozilla.components.concept.engine.InputResultDetail.Companion.OVERSCROLL_TOSTRING_DESCRIPTION
import mozilla.components.concept.engine.InputResultDetail.Companion.SCROLL_BOTTOM_TOSTRING_DESCRIPTION
import mozilla.components.concept.engine.InputResultDetail.Companion.SCROLL_IMPOSSIBLE_TOSTRING_DESCRIPTION
import mozilla.components.concept.engine.InputResultDetail.Companion.SCROLL_LEFT_TOSTRING_DESCRIPTION
import mozilla.components.concept.engine.InputResultDetail.Companion.SCROLL_RIGHT_TOSTRING_DESCRIPTION
import mozilla.components.concept.engine.InputResultDetail.Companion.SCROLL_TOP_TOSTRING_DESCRIPTION
import mozilla.components.concept.engine.InputResultDetail.Companion.SCROLL_TOSTRING_DESCRIPTION
import mozilla.components.concept.engine.InputResultDetail.Companion.TOSTRING_SEPARATOR
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test

class InputResultDetailTest {
    private lateinit var inputResultDetail: InputResultDetail

    @Before
    fun setup() {
        inputResultDetail = InputResultDetail.newInstance()
    }

    @Test
    fun `GIVEN InputResultDetail WHEN newInstance() is called with default parameters THEN a new instance with default values is returned`() {
        assertEquals(INPUT_HANDLING_UNKNOWN, inputResultDetail.inputResult)
        assertEquals(SCROLL_DIRECTIONS_NONE, inputResultDetail.scrollDirections)
        assertEquals(OVERSCROLL_DIRECTIONS_NONE, inputResultDetail.overscrollDirections)
    }

    @Test
    fun `GIVEN InputResultDetail WHEN newInstance() is called specifying overscroll enabled THEN a new instance with overscroll enabled is returned`() {
        inputResultDetail = InputResultDetail.newInstance(true)
        // Handling unknown but can overscroll. We need to preemptively allow for this,
        // otherwise pull to refresh would not work for the entirety of the touch.
        assertEquals(INPUT_HANDLING_UNKNOWN, inputResultDetail.inputResult)
        assertEquals(SCROLL_DIRECTIONS_NONE, inputResultDetail.scrollDirections)
        assertEquals(OVERSCROLL_DIRECTIONS_VERTICAL, inputResultDetail.overscrollDirections)
    }

    @Test
    fun `GIVEN an InputResultDetail instance WHEN copy is called with new values THEN the new values are set for the instance`() {
        inputResultDetail = inputResultDetail.copy(INPUT_HANDLED_CONTENT)
        assertEquals(INPUT_HANDLED_CONTENT, inputResultDetail.inputResult)
        assertEquals(SCROLL_DIRECTIONS_NONE, inputResultDetail.scrollDirections)
        assertEquals(OVERSCROLL_DIRECTIONS_NONE, inputResultDetail.overscrollDirections)

        inputResultDetail = inputResultDetail.copy(INPUT_HANDLED, SCROLL_DIRECTIONS_RIGHT)
        assertEquals(INPUT_HANDLED, inputResultDetail.inputResult)
        assertEquals(SCROLL_DIRECTIONS_RIGHT, inputResultDetail.scrollDirections)
        assertEquals(OVERSCROLL_DIRECTIONS_NONE, inputResultDetail.overscrollDirections)

        inputResultDetail = inputResultDetail.copy(
            INPUT_UNHANDLED,
            SCROLL_DIRECTIONS_NONE,
            OVERSCROLL_DIRECTIONS_HORIZONTAL,
        )
        assertEquals(INPUT_UNHANDLED, inputResultDetail.inputResult)
        assertEquals(SCROLL_DIRECTIONS_NONE, inputResultDetail.scrollDirections)
        assertEquals(OVERSCROLL_DIRECTIONS_HORIZONTAL, inputResultDetail.overscrollDirections)
    }

    @Test
    fun `GIVEN an InputResultDetail instance WHEN copy is called with new values THEN the invalid ones are filtered out`() {
        inputResultDetail = inputResultDetail.copy(42, 42, 42)
        assertEquals(INPUT_HANDLING_UNKNOWN, inputResultDetail.inputResult)
        assertEquals(SCROLL_DIRECTIONS_NONE, inputResultDetail.scrollDirections)
        assertEquals(OVERSCROLL_DIRECTIONS_NONE, inputResultDetail.overscrollDirections)
    }

    @Test
    fun `GIVEN an InputResultDetail instance with known touch handling WHEN copy is called with INPUT_HANDLING_UNKNOWN THEN this is not set`() {
        inputResultDetail = inputResultDetail.copy(INPUT_HANDLED_CONTENT)

        inputResultDetail = inputResultDetail.copy(INPUT_HANDLING_UNKNOWN)

        assertEquals(INPUT_HANDLED_CONTENT, inputResultDetail.inputResult)
    }

    @Test
    fun `GIVEN an InputResultDetail WHEN equals is called with another object of different type THEN it returns false`() {
        assertFalse(inputResultDetail == Any())
    }

    @Test
    fun `GIVEN an InputResultDetail WHEN equals is called with another instance with different values THEN it returns false`() {
        var differentInstance = InputResultDetail.newInstance(true)
        assertFalse(inputResultDetail == differentInstance)

        differentInstance = differentInstance.copy(SCROLL_DIRECTIONS_LEFT, OVERSCROLL_DIRECTIONS_NONE)
        assertFalse(inputResultDetail == differentInstance)

        differentInstance = differentInstance.copy(INPUT_HANDLED_CONTENT, SCROLL_DIRECTIONS_NONE)
        assertFalse(inputResultDetail == differentInstance)
    }

    @Test
    fun `GIVEN an InputResultDetail WHEN equals is called with another instance with equal values THEN it returns true`() {
        val equalValuesInstance = InputResultDetail.newInstance()

        assertTrue(inputResultDetail == equalValuesInstance)
    }

    @Test
    fun `GIVEN an InputResultDetail WHEN equals is called with the same instance THEN it returns true`() {
        assertTrue(inputResultDetail == inputResultDetail)
    }

    @Test
    fun `GIVEN an InputResultDetail WHEN hashCode is called for same values objects THEN it returns the same result`() {
        assertEquals(inputResultDetail.hashCode(), inputResultDetail.hashCode())

        assertEquals(inputResultDetail.hashCode(), InputResultDetail.newInstance().hashCode())

        inputResultDetail = inputResultDetail.copy(overscrollDirections = OVERSCROLL_DIRECTIONS_VERTICAL)
        assertEquals(inputResultDetail.hashCode(), InputResultDetail.newInstance(true).hashCode())
    }

    @Test
    fun `GIVEN an InputResultDetail WHEN hashCode is called for different values objects THEN it returns different results`() {
        assertNotEquals(inputResultDetail.hashCode(), InputResultDetail.newInstance(true).hashCode())

        inputResultDetail = inputResultDetail.copy(OVERSCROLL_DIRECTIONS_VERTICAL)
        assertNotEquals(inputResultDetail.hashCode(), InputResultDetail.newInstance().hashCode())

        inputResultDetail = inputResultDetail.copy(INPUT_HANDLED)
        assertNotEquals(inputResultDetail.hashCode(), InputResultDetail.newInstance().hashCode())
    }

    @Test
    fun `GIVEN an InputResultDetail WHEN toString is called THEN it returns a string referring to all data`() {
        // Add as many details as possible. Scroll and overscroll is not possible at the same time.
        inputResultDetail = inputResultDetail.copy(
            inputResult = INPUT_HANDLED,
            scrollDirections = SCROLL_DIRECTIONS_LEFT or SCROLL_DIRECTIONS_RIGHT or
                SCROLL_DIRECTIONS_TOP or SCROLL_DIRECTIONS_BOTTOM,
        )

        val result = inputResultDetail.toString()

        assertEquals(
            StringBuilder("InputResultDetail \$${inputResultDetail.hashCode()} (")
                .append("Input ${inputResultDetail.getInputResultHandledDescription()}. ")
                .append(
                    "Content ${inputResultDetail.getScrollDirectionsDescription()} " +
                        "and ${inputResultDetail.getOverscrollDirectionsDescription()}",
                )
                .append(')')
                .toString(),
            result,
        )
    }

    @Test
    fun `GIVEN an InputResultDetail WHEN getInputResultHandledDescription is called THEN returns a string describing who will handle the touch`() {
        assertEquals(INPUT_UNKNOWN_HANDLING_DESCRIPTION, inputResultDetail.getInputResultHandledDescription())

        assertEquals(
            INPUT_UNHANDLED_TOSTRING_DESCRIPTION,
            inputResultDetail.copy(INPUT_UNHANDLED).getInputResultHandledDescription(),
        )

        assertEquals(
            INPUT_HANDLED_TOSTRING_DESCRIPTION,
            inputResultDetail.copy(INPUT_HANDLED).getInputResultHandledDescription(),
        )

        assertEquals(
            INPUT_HANDLED_CONTENT_TOSTRING_DESCRIPTION,
            inputResultDetail.copy(INPUT_HANDLED_CONTENT).getInputResultHandledDescription(),
        )
    }

    @Test
    fun `GIVEN an InputResultDetail WHEN getScrollDirectionsDescription is called THEN it returns a string describing what scrolling is possible`() {
        assertEquals(SCROLL_IMPOSSIBLE_TOSTRING_DESCRIPTION, inputResultDetail.getScrollDirectionsDescription())

        inputResultDetail = inputResultDetail.copy(
            inputResult = INPUT_HANDLED,
            scrollDirections = SCROLL_DIRECTIONS_LEFT or SCROLL_DIRECTIONS_RIGHT or
                SCROLL_DIRECTIONS_TOP or SCROLL_DIRECTIONS_BOTTOM,
        )

        assertEquals(
            SCROLL_TOSTRING_DESCRIPTION +
                "$SCROLL_LEFT_TOSTRING_DESCRIPTION$TOSTRING_SEPARATOR" +
                "$SCROLL_TOP_TOSTRING_DESCRIPTION$TOSTRING_SEPARATOR" +
                "$SCROLL_RIGHT_TOSTRING_DESCRIPTION$TOSTRING_SEPARATOR" +
                SCROLL_BOTTOM_TOSTRING_DESCRIPTION,
            inputResultDetail.getScrollDirectionsDescription(),
        )
    }

    @Test
    fun `GIVEN an InputResultDetail WHEN getScrollDirectionsDescription is called for an unhandled touch THEN returns a string describing impossible scroll`() {
        assertEquals(SCROLL_IMPOSSIBLE_TOSTRING_DESCRIPTION, inputResultDetail.getScrollDirectionsDescription())

        inputResultDetail = inputResultDetail.copy(
            scrollDirections = SCROLL_DIRECTIONS_LEFT or SCROLL_DIRECTIONS_RIGHT or
                SCROLL_DIRECTIONS_TOP or SCROLL_DIRECTIONS_BOTTOM,
        )

        assertEquals(SCROLL_IMPOSSIBLE_TOSTRING_DESCRIPTION, inputResultDetail.getScrollDirectionsDescription())
    }

    @Test
    fun `GIVEN an InputResultDetail WHEN getOverscrollDirectionsDescription is called THEN it returns a string describing what overscrolling is possible`() {
        assertEquals(
            OVERSCROLL_IMPOSSIBLE_TOSTRING_DESCRIPTION,
            inputResultDetail.getOverscrollDirectionsDescription(),
        )

        inputResultDetail = inputResultDetail.copy(
            inputResult = INPUT_HANDLED,
            overscrollDirections = OVERSCROLL_DIRECTIONS_VERTICAL or OVERSCROLL_DIRECTIONS_HORIZONTAL,
        )

        assertEquals(
            OVERSCROLL_TOSTRING_DESCRIPTION +
                "$SCROLL_LEFT_TOSTRING_DESCRIPTION$TOSTRING_SEPARATOR" +
                "$SCROLL_TOP_TOSTRING_DESCRIPTION$TOSTRING_SEPARATOR" +
                "$SCROLL_RIGHT_TOSTRING_DESCRIPTION$TOSTRING_SEPARATOR" +
                SCROLL_BOTTOM_TOSTRING_DESCRIPTION,
            inputResultDetail.getOverscrollDirectionsDescription(),
        )
    }

    @Test
    fun `GIVEN an InputResultDetail WHEN getOverscrollDirectionsDescription is called for unhandled touch THEN returns a string describing impossible overscroll`() {
        assertEquals(
            OVERSCROLL_IMPOSSIBLE_TOSTRING_DESCRIPTION,
            inputResultDetail.getOverscrollDirectionsDescription(),
        )

        inputResultDetail = inputResultDetail.copy(
            inputResult = INPUT_HANDLED_CONTENT,
            overscrollDirections = OVERSCROLL_DIRECTIONS_VERTICAL or OVERSCROLL_DIRECTIONS_HORIZONTAL,
        )

        assertEquals(
            OVERSCROLL_IMPOSSIBLE_TOSTRING_DESCRIPTION,
            inputResultDetail.getOverscrollDirectionsDescription(),
        )
    }

    @Test
    fun `GIVEN an InputResultDetail instance WHEN isTouchHandlingUnknown is called THEN it returns true only if the inputResult is INPUT_HANDLING_UNKNOWN`() {
        assertTrue(inputResultDetail.isTouchHandlingUnknown())

        inputResultDetail = inputResultDetail.copy(INPUT_HANDLED)
        assertFalse(inputResultDetail.isTouchHandlingUnknown())

        inputResultDetail = inputResultDetail.copy(INPUT_HANDLED_CONTENT)
        assertFalse(inputResultDetail.isTouchHandlingUnknown())

        inputResultDetail = inputResultDetail.copy(INPUT_UNHANDLED)
        assertFalse(inputResultDetail.isTouchHandlingUnknown())
    }

    @Test
    fun `GIVEN an InputResultDetail instance WHEN isTouchHandledByBrowser is called THEN it returns true only if the inputResult is INPUT_HANDLED`() {
        assertFalse(inputResultDetail.isTouchHandledByBrowser())

        inputResultDetail = inputResultDetail.copy(INPUT_HANDLED)
        assertTrue(inputResultDetail.isTouchHandledByBrowser())

        inputResultDetail = inputResultDetail.copy(INPUT_HANDLED_CONTENT)
        assertFalse(inputResultDetail.isTouchHandledByBrowser())
    }

    @Test
    fun `GIVEN an InputResultDetail instance WHEN isTouchHandledByWebsite is called THEN it returns true only if the inputResult is INPUT_HANDLED_CONTENT`() {
        assertFalse(inputResultDetail.isTouchHandledByWebsite())

        inputResultDetail = inputResultDetail.copy(INPUT_HANDLED)
        assertFalse(inputResultDetail.isTouchHandledByWebsite())

        inputResultDetail = inputResultDetail.copy(INPUT_HANDLED_CONTENT)
        assertTrue(inputResultDetail.isTouchHandledByWebsite())
    }

    @Test
    fun `GIVEN an InputResultDetail instance WHEN isTouchUnhandled is called THEN it returns true only if the inputResult is INPUT_UNHANDLED`() {
        assertFalse(inputResultDetail.isTouchUnhandled())

        inputResultDetail = inputResultDetail.copy(INPUT_HANDLED)
        assertFalse(inputResultDetail.isTouchUnhandled())

        inputResultDetail = inputResultDetail.copy(INPUT_HANDLED_CONTENT)
        assertFalse(inputResultDetail.isTouchUnhandled())

        inputResultDetail = inputResultDetail.copy(INPUT_UNHANDLED)
        assertTrue(inputResultDetail.isTouchUnhandled())
    }

    @Test
    fun `GIVEN an InputResultDetail instance WHEN canScrollToLeft is called THEN it returns true only if the browser can scroll the page to left`() {
        assertFalse(inputResultDetail.canScrollToLeft())

        inputResultDetail = inputResultDetail.copy(INPUT_HANDLED)
        assertFalse(inputResultDetail.canScrollToLeft())

        inputResultDetail = inputResultDetail.copy(INPUT_HANDLED_CONTENT, SCROLL_DIRECTIONS_LEFT)
        assertFalse(inputResultDetail.canScrollToLeft())

        inputResultDetail = inputResultDetail.copy(INPUT_HANDLED)
        assertTrue(inputResultDetail.canScrollToLeft())

        inputResultDetail = inputResultDetail.copy(overscrollDirections = OVERSCROLL_DIRECTIONS_NONE)
        assertTrue(inputResultDetail.canScrollToLeft())
    }

    @Test
    fun `GIVEN an InputResultDetail instance WHEN canScrollToTop is called THEN it returns true only if the browser can scroll the page to top`() {
        assertFalse(inputResultDetail.canScrollToTop())

        inputResultDetail = inputResultDetail.copy(INPUT_HANDLED)
        assertFalse(inputResultDetail.canScrollToTop())

        inputResultDetail = inputResultDetail.copy(INPUT_HANDLED_CONTENT, SCROLL_DIRECTIONS_TOP)
        assertFalse(inputResultDetail.canScrollToTop())

        inputResultDetail = inputResultDetail.copy(INPUT_HANDLED)
        assertTrue(inputResultDetail.canScrollToTop())

        inputResultDetail = inputResultDetail.copy(overscrollDirections = OVERSCROLL_DIRECTIONS_VERTICAL)
        assertTrue(inputResultDetail.canScrollToTop())
    }

    @Test
    fun `GIVEN an InputResultDetail instance WHEN canScrollToRight is called THEN it returns true only if the browser can scroll the page to right`() {
        assertFalse(inputResultDetail.canScrollToRight())

        inputResultDetail = inputResultDetail.copy(INPUT_HANDLED)
        assertFalse(inputResultDetail.canScrollToRight())

        inputResultDetail = inputResultDetail.copy(INPUT_HANDLED_CONTENT, SCROLL_DIRECTIONS_RIGHT)
        assertFalse(inputResultDetail.canScrollToRight())

        inputResultDetail = inputResultDetail.copy(INPUT_HANDLED)
        assertTrue(inputResultDetail.canScrollToRight())

        inputResultDetail = inputResultDetail.copy(overscrollDirections = OVERSCROLL_DIRECTIONS_HORIZONTAL)
        assertTrue(inputResultDetail.canScrollToRight())
    }

    @Test
    fun `GIVEN an InputResultDetail instance WHEN canScrollToBottom is called THEN it returns true only if the browser can scroll the page to bottom`() {
        assertFalse(inputResultDetail.canScrollToBottom())

        inputResultDetail = inputResultDetail.copy(INPUT_HANDLED)
        assertFalse(inputResultDetail.canScrollToBottom())

        inputResultDetail = inputResultDetail.copy(INPUT_HANDLED_CONTENT, SCROLL_DIRECTIONS_BOTTOM)
        assertFalse(inputResultDetail.canScrollToBottom())

        inputResultDetail = inputResultDetail.copy(INPUT_HANDLED)
        assertTrue(inputResultDetail.canScrollToBottom())

        inputResultDetail = inputResultDetail.copy(overscrollDirections = OVERSCROLL_DIRECTIONS_NONE)
        assertTrue(inputResultDetail.canScrollToBottom())
    }

    @Test
    fun `GIVEN an InputResultDetail instance WHEN canOverscrollLeft is called THEN it returns true only in certain scenarios`() {
        // The scenarios (for which there is not enough space in the method name) being:
        //   - event is not handled by the webpage
        //   - webpage cannot be scrolled to the left in which case scroll would need to happen first
        //   - the content can be overscrolled to the left. Webpages can request overscroll to be disabled.

        assertFalse(inputResultDetail.canOverscrollLeft())

        inputResultDetail = inputResultDetail.copy(INPUT_HANDLED, SCROLL_DIRECTIONS_BOTTOM, OVERSCROLL_DIRECTIONS_HORIZONTAL)
        assertTrue(inputResultDetail.canOverscrollLeft())

        inputResultDetail = inputResultDetail.copy(INPUT_UNHANDLED)
        assertTrue(inputResultDetail.canOverscrollLeft())

        inputResultDetail = inputResultDetail.copy(scrollDirections = SCROLL_DIRECTIONS_LEFT)
        assertFalse(inputResultDetail.canOverscrollLeft())

        inputResultDetail = inputResultDetail.copy(scrollDirections = SCROLL_DIRECTIONS_TOP, overscrollDirections = OVERSCROLL_DIRECTIONS_HORIZONTAL)
        assertTrue(inputResultDetail.canOverscrollLeft())

        inputResultDetail = inputResultDetail.copy(INPUT_HANDLED, SCROLL_DIRECTIONS_RIGHT, OVERSCROLL_DIRECTIONS_VERTICAL)
        assertFalse(inputResultDetail.canOverscrollLeft())
    }

    @Test
    fun `GIVEN an InputResultDetail instance WHEN canOverscrollTop is called THEN it returns true only in certain scenarios`() {
        // The scenarios (for which there is not enough space in the method name) being:
        //   - event is not handled by the webpage
        //   - webpage cannot be scrolled to the top in which case scroll would need to happen first
        //   - the content can be overscrolled to the top. Webpages can request overscroll to be disabled.

        assertFalse(inputResultDetail.canOverscrollTop())

        inputResultDetail = inputResultDetail.copy(INPUT_HANDLED_CONTENT)
        assertFalse(inputResultDetail.canOverscrollTop())

        inputResultDetail = inputResultDetail.copy(INPUT_HANDLED)
        assertFalse(inputResultDetail.canOverscrollTop())

        inputResultDetail = inputResultDetail.copy(INPUT_HANDLED_CONTENT, SCROLL_DIRECTIONS_TOP, OVERSCROLL_DIRECTIONS_VERTICAL)
        assertFalse(inputResultDetail.canOverscrollTop())

        inputResultDetail = inputResultDetail.copy(INPUT_UNHANDLED, SCROLL_DIRECTIONS_LEFT, OVERSCROLL_DIRECTIONS_VERTICAL)
        assertTrue(inputResultDetail.canOverscrollTop())

        inputResultDetail = inputResultDetail.copy(overscrollDirections = OVERSCROLL_DIRECTIONS_HORIZONTAL)
        assertFalse(inputResultDetail.canOverscrollTop())
    }

    @Test
    fun `GIVEN an InputResultDetail instance WHEN canOverscrollRight is called THEN it returns true only in certain scenarios`() {
        // The scenarios (for which there is not enough space in the method name) being:
        //   - event is not handled by the webpage
        //   - webpage cannot be scrolled to the right in which case scroll would need to happen first
        //   - the content can be overscrolled to the right. Webpages can request overscroll to be disabled.

        assertFalse(inputResultDetail.canOverscrollRight())

        inputResultDetail = inputResultDetail.copy(INPUT_HANDLED, SCROLL_DIRECTIONS_BOTTOM, OVERSCROLL_DIRECTIONS_HORIZONTAL)
        assertTrue(inputResultDetail.canOverscrollRight())

        inputResultDetail = inputResultDetail.copy(INPUT_UNHANDLED)
        assertTrue(inputResultDetail.canOverscrollRight())

        inputResultDetail = inputResultDetail.copy(scrollDirections = SCROLL_DIRECTIONS_RIGHT)
        assertFalse(inputResultDetail.canOverscrollRight())

        inputResultDetail = inputResultDetail.copy(scrollDirections = SCROLL_DIRECTIONS_TOP, overscrollDirections = OVERSCROLL_DIRECTIONS_HORIZONTAL)
        assertTrue(inputResultDetail.canOverscrollRight())

        inputResultDetail = inputResultDetail.copy(INPUT_HANDLED, SCROLL_DIRECTIONS_LEFT, OVERSCROLL_DIRECTIONS_VERTICAL)
        assertFalse(inputResultDetail.canOverscrollRight())
    }

    @Test
    fun `GIVEN an InputResultDetail instance WHEN canOverscrollBottom is called THEN it returns true only in certain scenarios`() {
        // The scenarios (for which there is not enough space in the method name) being:
        //   - event is not handled by the webpage
        //   - webpage cannot be scrolled to the bottom in which case scroll would need to happen first
        //   - the content can be overscrolled to the bottom. Webpages can request overscroll to be disabled.

        assertFalse(inputResultDetail.canOverscrollBottom())

        inputResultDetail = inputResultDetail.copy(INPUT_HANDLED_CONTENT)
        assertFalse(inputResultDetail.canOverscrollBottom())

        inputResultDetail = inputResultDetail.copy(INPUT_HANDLED)
        assertFalse(inputResultDetail.canOverscrollBottom())

        inputResultDetail = inputResultDetail.copy(INPUT_HANDLED_CONTENT, SCROLL_DIRECTIONS_BOTTOM, OVERSCROLL_DIRECTIONS_VERTICAL)
        assertFalse(inputResultDetail.canOverscrollBottom())

        inputResultDetail = inputResultDetail.copy(INPUT_UNHANDLED, SCROLL_DIRECTIONS_LEFT, OVERSCROLL_DIRECTIONS_VERTICAL)
        assertTrue(inputResultDetail.canOverscrollBottom())

        inputResultDetail = inputResultDetail.copy(overscrollDirections = OVERSCROLL_DIRECTIONS_HORIZONTAL)
        assertFalse(inputResultDetail.canOverscrollBottom())
    }
}
