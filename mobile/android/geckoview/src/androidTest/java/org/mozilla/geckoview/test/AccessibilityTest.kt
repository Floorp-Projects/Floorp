/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDisplay
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDevToolsAPI

import android.os.Build
import android.os.Bundle

import android.support.test.filters.MediumTest
import android.support.test.InstrumentationRegistry
import android.support.test.runner.AndroidJUnit4

import android.view.accessibility.AccessibilityNodeInfo
import android.view.accessibility.AccessibilityNodeProvider
import android.view.accessibility.AccessibilityEvent
import android.view.accessibility.AccessibilityRecord
import android.view.View
import android.view.ViewGroup

import android.widget.FrameLayout

import org.hamcrest.Matchers.*
import org.junit.Test
import org.junit.Before
import org.junit.After
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
@MediumTest
@WithDisplay(width = 480, height = 640)
@WithDevToolsAPI
class AccessibilityTest : BaseSessionTest() {
    lateinit var view: View
    val provider: AccessibilityNodeProvider get() = view.accessibilityNodeProvider

    // Given a child ID, return the virtual descendent ID.
    private fun getVirtualDescendantId(childId: Long): Int {
        try {
            val getVirtualDescendantIdMethod =
                AccessibilityNodeInfo::class.java.getMethod("getVirtualDescendantId", Long::class.java)
            return getVirtualDescendantIdMethod.invoke(null, childId) as Int
        } catch (ex: Exception) {
            return 0
        }
    }

    // Retrieve the virtual descendent ID of the event's source.
    private fun getSourceId(event: AccessibilityEvent): Int {
        try {
            val getSourceIdMethod =
                AccessibilityRecord::class.java.getMethod("getSourceNodeId")
            return getVirtualDescendantId(getSourceIdMethod.invoke(event) as Long)
        } catch (ex: Exception) {
            return 0
        }
    }

    private interface EventDelegate {
        fun onAccessibilityFocused(event: AccessibilityEvent) { }
        fun onFocused(event: AccessibilityEvent) { }
        fun onTextSelectionChanged(event: AccessibilityEvent) { }
        fun onTextChanged(event: AccessibilityEvent) { }
        fun onTextTraversal(event: AccessibilityEvent) { }
    }

    @Before fun setup() {
        // We initialize a view with a parent and grandparent so that the
        // accessibility events propagate up at least to the parent.
        view = FrameLayout(InstrumentationRegistry.getTargetContext())
        FrameLayout(InstrumentationRegistry.getTargetContext()).addView(view)
        FrameLayout(InstrumentationRegistry.getTargetContext()).addView(view.parent as View)

        // Force on accessibility and assign the session's accessibility
        // object a view.
        sessionRule.setPrefsUntilTestEnd(mapOf("accessibility.force_disabled" to -1))
        mainSession.accessibility.view = view

        // Set up an external delegate that will intercept accessibility events.
        sessionRule.addExternalDelegateUntilTestEnd(
            EventDelegate::class,
        { newDelegate -> (view.parent as View).setAccessibilityDelegate(object : View.AccessibilityDelegate() {
            override fun onRequestSendAccessibilityEvent(host: ViewGroup, child: View, event: AccessibilityEvent): Boolean {
                when (event.eventType) {
                    AccessibilityEvent.TYPE_VIEW_FOCUSED -> newDelegate.onFocused(event)
                    AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED -> newDelegate.onAccessibilityFocused(event)
                    AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED -> newDelegate.onTextSelectionChanged(event)
                    AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED -> newDelegate.onTextChanged(event)
                    AccessibilityEvent.TYPE_VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY -> newDelegate.onTextTraversal(event)
                    else -> {}
                }
                return false
            }
        }) },
        { (view.parent as View).setAccessibilityDelegate(null) },
        object : EventDelegate { })
    }

    @After fun teardown() {
        sessionRule.session.accessibility.view = null
    }

    @Test fun testRootNode() {
        assertThat("provider is not null", provider, notNullValue())
        val node = provider.createAccessibilityNodeInfo(AccessibilityNodeProvider.HOST_VIEW_ID)
        assertThat("Root node should have WebView class name",
            node.className.toString(), equalTo("android.webkit.WebView"))
    }

    @Test fun testPageLoad() {
        sessionRule.session.loadTestPath(INPUTS_PATH)

        sessionRule.waitUntilCalled(object : EventDelegate {
            @AssertCalled(count = 1)
            override fun onFocused(event: AccessibilityEvent) { }
        })
    }

    @Test fun testAccessibilityFocus() {
        var nodeId = AccessibilityNodeProvider.HOST_VIEW_ID
        sessionRule.session.loadTestPath(INPUTS_PATH)
        sessionRule.waitForPageStop()

        provider.performAction(AccessibilityNodeProvider.HOST_VIEW_ID,
            AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS, null)

        sessionRule.waitUntilCalled(object : EventDelegate {
            @AssertCalled(count = 1)
            override fun onAccessibilityFocused(event: AccessibilityEvent) {
                nodeId = getSourceId(event)
                val node = provider.createAccessibilityNodeInfo(nodeId)
                assertThat("Text node should not be focusable", node.isFocusable, equalTo(false))
            }
        })

        provider.performAction(nodeId,
            AccessibilityNodeInfo.ACTION_NEXT_HTML_ELEMENT, null)

        sessionRule.waitUntilCalled(object : EventDelegate {
            @AssertCalled(count = 1)
            override fun onAccessibilityFocused(event: AccessibilityEvent) {
                nodeId = getSourceId(event)
                val node = provider.createAccessibilityNodeInfo(nodeId)
                assertThat("Entry node should be focusable", node.isFocusable, equalTo(true))
            }
        })
    }

    @Test fun testTextEntryNode() {
        sessionRule.session.loadString("<input aria-label='Name' value='Tobias'>", "text/html")
        sessionRule.waitForPageStop()

        mainSession.evaluateJS("$('input').focus()")

        sessionRule.waitUntilCalled(object : EventDelegate {
            @AssertCalled(count = 1)
            override fun onAccessibilityFocused(event: AccessibilityEvent) {
                val nodeId = getSourceId(event)
                val node = provider.createAccessibilityNodeInfo(nodeId)
                assertThat("Focused EditBox", node.className.toString(),
                        equalTo("android.widget.EditText"))
                if (Build.VERSION.SDK_INT >= 19) {
                    assertThat("Hint has field name",
                            node.extras.getString("AccessibilityNodeInfo.hint"),
                            equalTo("Name"))
                }
            }
        })
    }

    private fun waitUntilTextSelectionChanged(fromIndex: Int, toIndex: Int) {
        var eventFromIndex = 0;
        var eventToIndex = 0;
        do {
            sessionRule.waitUntilCalled(object : EventDelegate {
                override fun onTextSelectionChanged(event: AccessibilityEvent) {
                    eventFromIndex = event.fromIndex;
                    eventToIndex = event.toIndex;
                }
            })
        } while (fromIndex != eventFromIndex || toIndex != eventToIndex)
    }

    private fun waitUntilTextTraversed(fromIndex: Int, toIndex: Int) {
        sessionRule.waitUntilCalled(object : EventDelegate {
            @AssertCalled(count = 1)
            override fun onTextTraversal(event: AccessibilityEvent) {
              assertThat("fromIndex matches", event.fromIndex, equalTo(fromIndex))
              assertThat("toIndex matches", event.toIndex, equalTo(toIndex))
            }
        })
    }

    private fun setSelectionArguments(start: Int, end: Int): Bundle {
        val arguments = Bundle(2)
        arguments.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_START_INT, start)
        arguments.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_END_INT, end)
        return arguments
    }

    private fun moveByGranularityArguments(granularity: Int, extendSelection: Boolean = false): Bundle {
        val arguments = Bundle(2)
        arguments.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT, granularity)
        arguments.putBoolean(AccessibilityNodeInfo.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN, extendSelection)
        return arguments
    }

    @Test fun testClipboard() {
        var nodeId = AccessibilityNodeProvider.HOST_VIEW_ID;
        sessionRule.session.loadString("<input value='hello cruel world' id='input'>", "text/html")
        sessionRule.waitForPageStop()

        mainSession.evaluateJS("$('input').focus()")

        sessionRule.waitUntilCalled(object : EventDelegate {
            @AssertCalled(count = 1)
            override fun onAccessibilityFocused(event: AccessibilityEvent) {
                nodeId = getSourceId(event)
                val node = provider.createAccessibilityNodeInfo(nodeId)
                assertThat("Focused EditBox", node.className.toString(),
                        equalTo("android.widget.EditText"))
            }

            @AssertCalled(count = 1)
            override fun onTextSelectionChanged(event: AccessibilityEvent) {
                assertThat("fromIndex should be at start", event.fromIndex, equalTo(0))
                assertThat("toIndex should be at start", event.toIndex, equalTo(0))
            }
        })

        provider.performAction(nodeId, AccessibilityNodeInfo.ACTION_SET_SELECTION, setSelectionArguments(5, 11))
        waitUntilTextSelectionChanged(5, 11)

        provider.performAction(nodeId, AccessibilityNodeInfo.ACTION_COPY, null)

        provider.performAction(nodeId, AccessibilityNodeInfo.ACTION_SET_SELECTION, setSelectionArguments(11, 11))
        waitUntilTextSelectionChanged(11, 11)

        provider.performAction(nodeId, AccessibilityNodeInfo.ACTION_PASTE, null)
        sessionRule.waitUntilCalled(object : EventDelegate {
            @AssertCalled(count = 1)
            override fun onTextChanged(event: AccessibilityEvent) {
                assertThat("text should be pasted", event.text[0].toString(), equalTo("hello cruel cruel world"))
            }
        })

        provider.performAction(nodeId, AccessibilityNodeInfo.ACTION_SET_SELECTION, setSelectionArguments(17, 23))
        waitUntilTextSelectionChanged(17, 23)

        provider.performAction(nodeId, AccessibilityNodeInfo.ACTION_PASTE, null)
        sessionRule.waitUntilCalled(object : EventDelegate {
            @AssertCalled
            override fun onTextChanged(event: AccessibilityEvent) {
                assertThat("text should be pasted", event.text[0].toString(), equalTo("hello cruel cruel cruel"))
            }
        })
    }

    @Test fun testMoveByCharacter() {
        var nodeId = AccessibilityNodeProvider.HOST_VIEW_ID
        sessionRule.session.loadTestPath(LOREM_IPSUM_HTML_PATH)
        sessionRule.waitForPageStop()

        provider.performAction(AccessibilityNodeProvider.HOST_VIEW_ID,
                AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS, null)

        sessionRule.waitUntilCalled(object : EventDelegate {
            @AssertCalled(count = 1)
            override fun onAccessibilityFocused(event: AccessibilityEvent) {
                nodeId = getSourceId(event)
                val node = provider.createAccessibilityNodeInfo(nodeId)
                assertThat("Accessibility focus on first paragraph", node.text as String, startsWith("Lorem ipsum"))
            }
        })

        provider.performAction(nodeId,
                AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY,
                moveByGranularityArguments(AccessibilityNodeInfo.MOVEMENT_GRANULARITY_CHARACTER))
        waitUntilTextTraversed(0, 1) // "L"

        provider.performAction(nodeId,
                AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY,
                moveByGranularityArguments(AccessibilityNodeInfo.MOVEMENT_GRANULARITY_CHARACTER))
        waitUntilTextTraversed(1, 2) // "o"

        provider.performAction(nodeId,
                AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY,
                moveByGranularityArguments(AccessibilityNodeInfo.MOVEMENT_GRANULARITY_CHARACTER))
        waitUntilTextTraversed(0, 1) // "L"
    }

    @Test fun testMoveByWord() {
        var nodeId = AccessibilityNodeProvider.HOST_VIEW_ID
        sessionRule.session.loadTestPath(LOREM_IPSUM_HTML_PATH)
        sessionRule.waitForPageStop()

        provider.performAction(AccessibilityNodeProvider.HOST_VIEW_ID,
                AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS, null)

        sessionRule.waitUntilCalled(object : EventDelegate {
            @AssertCalled(count = 1)
            override fun onAccessibilityFocused(event: AccessibilityEvent) {
                nodeId = getSourceId(event)
                val node = provider.createAccessibilityNodeInfo(nodeId)
                assertThat("Accessibility focus on first paragraph", node.text as String, startsWith("Lorem ipsum"))
            }
        })

        provider.performAction(nodeId,
                AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY,
                moveByGranularityArguments(AccessibilityNodeInfo.MOVEMENT_GRANULARITY_WORD))
        waitUntilTextTraversed(0, 5) // "Lorem"

        provider.performAction(nodeId,
                AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY,
                moveByGranularityArguments(AccessibilityNodeInfo.MOVEMENT_GRANULARITY_WORD))
        waitUntilTextTraversed(6, 11) // "ipsum"

        provider.performAction(nodeId,
                AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY,
                moveByGranularityArguments(AccessibilityNodeInfo.MOVEMENT_GRANULARITY_WORD))
        waitUntilTextTraversed(0, 5) // "Lorem"
    }

    @Test fun testMoveByLine() {
        var nodeId = AccessibilityNodeProvider.HOST_VIEW_ID
        sessionRule.session.loadTestPath(LOREM_IPSUM_HTML_PATH)
        sessionRule.waitForPageStop()

        provider.performAction(AccessibilityNodeProvider.HOST_VIEW_ID,
                AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS, null)

        sessionRule.waitUntilCalled(object : EventDelegate {
            @AssertCalled(count = 1)
            override fun onAccessibilityFocused(event: AccessibilityEvent) {
                nodeId = getSourceId(event)
                val node = provider.createAccessibilityNodeInfo(nodeId)
                assertThat("Accessibility focus on first paragraph", node.text as String, startsWith("Lorem ipsum"))
            }
        })

        provider.performAction(nodeId,
                AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY,
                moveByGranularityArguments(AccessibilityNodeInfo.MOVEMENT_GRANULARITY_LINE))
        waitUntilTextTraversed(0, 18) // "Lorem ipsum dolor "

        provider.performAction(nodeId,
                AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY,
                moveByGranularityArguments(AccessibilityNodeInfo.MOVEMENT_GRANULARITY_LINE))
        waitUntilTextTraversed(18, 28) // "sit amet, "

        provider.performAction(nodeId,
                AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY,
                moveByGranularityArguments(AccessibilityNodeInfo.MOVEMENT_GRANULARITY_LINE))
        waitUntilTextTraversed(0, 18) // "Lorem ipsum dolor "
    }
}
