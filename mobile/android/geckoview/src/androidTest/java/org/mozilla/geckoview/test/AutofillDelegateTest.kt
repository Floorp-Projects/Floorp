/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import android.graphics.Rect
import android.util.SparseArray
import android.view.KeyEvent
import android.view.View
import androidx.test.filters.MediumTest
import org.hamcrest.Matchers.* // ktlint-disable no-wildcard-imports
import org.junit.Assume.assumeThat
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.Parameterized
import org.mozilla.geckoview.Autofill
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSession.TextInputDelegate
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.* // ktlint-disable no-wildcard-imports

@RunWith(Parameterized::class)
@MediumTest
class AutofillDelegateTest : BaseSessionTest() {

    companion object {
        @get:Parameterized.Parameters(name = "{0}")
        @JvmStatic
        val parameters: List<Array<out Any>> = listOf(
            arrayOf("#inProcess"),
            arrayOf("#oop"),
        )
    }

    @field:Parameterized.Parameter(0)
    @JvmField
    var iframe: String = ""

    // Whether the iframe is loaded in-process (i.e. with the same origin as the
    // outer html page) or out-of-process.
    private val pageUrl by lazy {
        when (iframe) {
            "#inProcess" -> "http://example.org/tests/junit/forms_xorigin.html"
            "#oop" -> createTestUrl(FORMS_XORIGIN_HTML_PATH)
            else -> throw IllegalStateException()
        }
    }

    @Test fun autofillCommit() {
        sessionRule.setPrefsUntilTestEnd(
            mapOf(
                "signon.rememberSignons" to true,
                "signon.userInputRequiredToCapture.enabled" to false,
            ),
        )

        mainSession.loadUri(pageUrl)
        // Wait for the auto-fill nodes to populate.
        sessionRule.waitUntilCalled(object : Autofill.Delegate, GeckoSession.ProgressDelegate {
            // We expect to get a call to onSessionStart and many calls to onNodeAdd depending
            // on timing.
            @AssertCalled(count = 1)
            override fun onSessionStart(session: GeckoSession) {}

            @AssertCalled(count = -1)
            override fun onNodeAdd(
                session: GeckoSession,
                node: Autofill.Node,
                data: Autofill.NodeData,
            ) {}

            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {}
        })

        // Assign node values.
        mainSession.evaluateJS("document.querySelector('#user1').value = 'user1x'")
        mainSession.evaluateJS("document.querySelector('#pass1').value = 'pass1x'")
        mainSession.evaluateJS("document.querySelector('#email1').value = 'e@mail.com'")
        mainSession.evaluateJS("document.querySelector('#number1').value = '1'")

        // Submit the session.
        mainSession.evaluateJS("document.querySelector('#form1').submit()")

        sessionRule.waitUntilCalled(object : Autofill.Delegate {
            @AssertCalled(order = [1, 2, 3, 4])
            override fun onNodeUpdate(
                session: GeckoSession,
                node: Autofill.Node,
                data: Autofill.NodeData,
            ) {
            }

            @AssertCalled(order = [5])
            override fun onSessionCommit(
                session: GeckoSession,
                node: Autofill.Node,
                data: Autofill.NodeData,
            ) {
                val autofillSession = mainSession.autofillSession
                assertThat(
                    "Values should match",
                    countAutofillNodes({
                        autofillSession.dataFor(it).value == "user1x"
                    }),
                    equalTo(1),
                )
                assertThat(
                    "Values should match",
                    countAutofillNodes({
                        autofillSession.dataFor(it).value == "pass1x"
                    }),
                    equalTo(1),
                )
                assertThat(
                    "Values should match",
                    countAutofillNodes({
                        autofillSession.dataFor(it).value == "e@mail.com"
                    }),
                    equalTo(1),
                )
                assertThat(
                    "Values should match",
                    countAutofillNodes({
                        autofillSession.dataFor(it).value == "1"
                    }),
                    equalTo(1),
                )
            }
        })
    }

    @Test fun autofillCommitIdValue() {
        sessionRule.setPrefsUntilTestEnd(
            mapOf(
                "signon.rememberSignons" to true,
                "signon.userInputRequiredToCapture.enabled" to false,
            ),
        )

        mainSession.loadTestPath(FORMS_ID_VALUE_HTML_PATH)
        // Wait for the auto-fill nodes to populate.
        sessionRule.waitUntilCalled(object : Autofill.Delegate, GeckoSession.ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onSessionStart(session: GeckoSession) {}

            @AssertCalled(count = -1)
            override fun onNodeAdd(
                session: GeckoSession,
                node: Autofill.Node,
                data: Autofill.NodeData,
            ) {}

            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {}
        })

        // Assign node values.
        mainSession.evaluateJS("document.querySelector('#value').value = 'pass1x'")

        // Submit the session.
        mainSession.evaluateJS("document.querySelector('#form1').submit()")

        sessionRule.waitUntilCalled(object : Autofill.Delegate {
            @AssertCalled(order = [1])
            override fun onNodeUpdate(
                session: GeckoSession,
                node: Autofill.Node,
                data: Autofill.NodeData,
            ) {
            }

            @AssertCalled(order = [2])
            override fun onSessionCommit(
                session: GeckoSession,
                node: Autofill.Node,
                data: Autofill.NodeData,
            ) {
                assertThat(
                    "Values should match",
                    countAutofillNodes({
                        mainSession.autofillSession.dataFor(it).value == "pass1x"
                    }),
                    equalTo(1),
                )
            }
        })
    }

    @Test fun autofill() {
        // Test parts of the Oreo auto-fill API; there is another autofill test in
        // SessionAccessibility for a11y auto-fill support.
        mainSession.loadUri(pageUrl)
        // Wait for the auto-fill nodes to populate.
        sessionRule.waitUntilCalled(object : Autofill.Delegate, GeckoSession.ProgressDelegate {
            // We expect many call to onNodeAdd while loading the page
            @AssertCalled(count = -1)
            override fun onNodeAdd(
                session: GeckoSession,
                node: Autofill.Node,
                data: Autofill.NodeData,
            ) {}

            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {}
        })

        val autofills = mapOf(
            "#user1" to "bar",
            "#user2" to "bar",
            "#pass1" to "baz",
            "#pass2" to "baz",
            "#email1" to "a@b.c",
            "#number1" to "24",
            "#tel1" to "42",
        )

        // Set up promises to monitor the values changing.
        val promises = autofills.map { entry ->
            // Repeat each test with both the top document and the iframe document.
            mainSession.evaluatePromiseJS(
                """
                window.getDataForAllFrames('${entry.key}', '${entry.value}')
                """,
            )
        }

        val autofillValues = SparseArray<CharSequence>()

        // Perform auto-fill and return number of auto-fills performed.
        fun checkAutofillChild(child: Autofill.Node, domain: String) {
            // Seal the node info instance so we can perform actions on it.
            if (child.children.isNotEmpty()) {
                for (c in child.children) {
                    checkAutofillChild(c!!, child.domain)
                }
            }

            if (child == mainSession.autofillSession.root) {
                return
            }

            assertThat(
                "Should have HTML tag",
                child.tag,
                not(isEmptyOrNullString()),
            )
            if (domain != "") {
                assertThat(
                    "Web domain should match its parent.",
                    child.domain,
                    equalTo(domain),
                )
            }

            if (child.inputType == Autofill.InputType.TEXT) {
                assertThat("Input should be enabled", child.enabled, equalTo(true))
                assertThat(
                    "Input should be focusable",
                    child.focusable,
                    equalTo(true),
                )

                assertThat("Should have HTML tag", child.tag, equalTo("input"))
                assertThat("Should have ID attribute", child.attributes.get("id"), not(isEmptyOrNullString()))
            }

            val childId = mainSession.autofillSession.dataFor(child).id
            autofillValues.append(
                childId,
                when (child.inputType) {
                    Autofill.InputType.NUMBER -> "24"
                    Autofill.InputType.PHONE -> "42"
                    Autofill.InputType.TEXT -> when (child.hint) {
                        Autofill.Hint.PASSWORD -> "baz"
                        Autofill.Hint.EMAIL_ADDRESS -> "a@b.c"
                        else -> "bar"
                    }
                    else -> "bar"
                },
            )
        }

        val nodes = mainSession.autofillSession.root
        checkAutofillChild(nodes, "")

        mainSession.autofillSession.autofill(autofillValues)

        // Wait on the promises and check for correct values.
        for (values in promises.map { it.value.asJsonArray() }) {
            for (i in 0 until values.length()) {
                val (key, actual, expected, eventInterface) = values.get(i).asJSList<String>()

                assertThat("Auto-filled value must match ($key)", actual, equalTo(expected))
                assertThat(
                    "input event should be dispatched with InputEvent interface",
                    eventInterface,
                    equalTo("InputEvent"),
                )
            }
        }
    }

    @Test fun autofillUnknownValue() {
        // Test parts of the Oreo auto-fill API; there is another autofill test in
        // SessionAccessibility for a11y auto-fill support.
        mainSession.loadUri(pageUrl)
        // Wait for the auto-fill nodes to populate.
        sessionRule.waitUntilCalled(object : Autofill.Delegate, GeckoSession.ProgressDelegate {
            @AssertCalled(count = -1)
            override fun onNodeAdd(
                session: GeckoSession,
                node: Autofill.Node,
                data: Autofill.NodeData,
            ) {}

            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {}
        })

        val autofillValues = SparseArray<CharSequence>()
        autofillValues.append(-1, "lobster")
        mainSession.autofillSession.autofill(autofillValues)
    }

    private fun countAutofillNodes(
        cond: (Autofill.Node) -> Boolean =
            { it.inputType != Autofill.InputType.NONE },
        root: Autofill.Node? = null,
    ): Int {
        val node = if (root !== null) root else mainSession.autofillSession.root
        return (if (cond(node)) 1 else 0) +
            node.children.sumOf {
                countAutofillNodes(cond, it)
            }
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun autofillNavigation() {
        // Wait for the accessibility nodes to populate.
        mainSession.loadUri(pageUrl)

        sessionRule.waitUntilCalled(object :
            Autofill.Delegate,
            ShouldContinue,
            GeckoSession.ProgressDelegate {
            var nodeCount = 0

            // Continue waiting util we get all 16 nodes
            override fun shouldContinue(): Boolean = nodeCount < 16

            @AssertCalled(count = 1)
            override fun onSessionStart(session: GeckoSession) {}

            @AssertCalled(count = -1)
            override fun onNodeAdd(
                session: GeckoSession,
                node: Autofill.Node,
                data: Autofill.NodeData,
            ) {
                assertThat("Node should be valid", node, notNullValue())
                nodeCount = countAutofillNodes()
            }

            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {}
        })

        assertThat(
            "Initial auto-fill count should match",
            countAutofillNodes(),
            equalTo(16),
        )

        // Now wait for the nodes to clear.
        mainSession.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitUntilCalled(object : Autofill.Delegate, GeckoSession.ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onSessionCancel(session: GeckoSession) {}

            @AssertCalled
            override fun onPageStop(session: GeckoSession, success: Boolean) {}
        })

        assertThat(
            "Should not have auto-fill fields",
            countAutofillNodes(),
            equalTo(0),
        )

        mainSession.goBack()
        sessionRule.waitUntilCalled(object :
            Autofill.Delegate,
            GeckoSession.ProgressDelegate,
            ShouldContinue {
            var nodeCount = 0
            override fun shouldContinue(): Boolean = nodeCount < 16

            @AssertCalled(count = 1)
            override fun onSessionStart(session: GeckoSession) {}

            @AssertCalled(count = -1)
            override fun onNodeAdd(
                session: GeckoSession,
                node: Autofill.Node,
                data: Autofill.NodeData,
            ) {
                assertThat("Node should be valid", node, notNullValue())
                nodeCount = countAutofillNodes()
            }

            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {}
        })

        assertThat(
            "Should have auto-fill fields again",
            countAutofillNodes(),
            equalTo(16),
        )

        var focused = mainSession.autofillSession.focused
        assertThat(
            "Should not have focused field",
            countAutofillNodes({ it == focused }),
            equalTo(0),
        )

        mainSession.evaluateJS("document.querySelector('#pass2').focus()")

        sessionRule.waitUntilCalled(object : Autofill.Delegate {
            @AssertCalled(count = 1)
            override fun onNodeFocus(
                session: GeckoSession,
                node: Autofill.Node,
                data: Autofill.NodeData,
            ) {
                assertThat("ID should be valid", node, notNullValue())
            }
        })

        focused = mainSession.autofillSession.focused
        assertThat(
            "Should have one focused field",
            countAutofillNodes({ it == focused }),
            equalTo(1),
        )
        // The focused field, its siblings, its parent, and the root node should
        // be visible.
        // Hidden elements are ignored.
        // TODO: Is this actually correct? Should the whole focused branch be
        // visible or just the nodes as described above?
        assertThat(
            "Should have nine visible nodes",
            countAutofillNodes({ node -> mainSession.autofillSession.isVisible(node) }),
            equalTo(8),
        )

        mainSession.evaluateJS("document.querySelector('#pass2').blur()")
        sessionRule.waitUntilCalled(object : Autofill.Delegate {
            @AssertCalled(count = 1)
            override fun onNodeBlur(
                session: GeckoSession,
                node: Autofill.Node,
                data: Autofill.NodeData,
            ) {
                assertThat("ID should be valid", node, notNullValue())
            }
        })

        focused = mainSession.autofillSession.focused
        assertThat(
            "Should not have focused field",
            countAutofillNodes({ it == focused }),
            equalTo(0),
        )
    }

    @WithDisplay(height = 100, width = 100)
    @Test
    fun autofillUserpass() {
        mainSession.loadTestPath(FORMS2_HTML_PATH)
        // Wait for the auto-fill nodes to populate.
        sessionRule.waitUntilCalled(object : Autofill.Delegate, GeckoSession.ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onSessionStart(session: GeckoSession) {}

            @AssertCalled(count = 1)
            override fun onNodeFocus(
                session: GeckoSession,
                node: Autofill.Node,
                data: Autofill.NodeData,
            ) {}

            @AssertCalled(count = -1)
            override fun onNodeAdd(
                session: GeckoSession,
                node: Autofill.Node,
                data: Autofill.NodeData,
            ) {}

            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {}
        })

        // Perform auto-fill and return number of auto-fills performed.
        fun checkAutofillChild(child: Autofill.Node): Int {
            var sum = 0
            // Seal the node info instance so we can perform actions on it.
            for (c in child.children) {
                sum += checkAutofillChild(c!!)
            }

            if (child.hint == Autofill.Hint.NONE) {
                return sum
            }

            val childId = mainSession.autofillSession.dataFor(child).id
            assertThat("ID should be valid", childId, not(equalTo(View.NO_ID)))
            assertThat("Should have HTML tag", child.tag, equalTo("input"))

            return sum + 1
        }

        val root = mainSession.autofillSession.root

        // form and iframe have each have 2 nodes with hints.
        assertThat(
            "autofill hint count",
            checkAutofillChild(root),
            equalTo(4),
        )
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun autofillActiveChange() {
        // We should blur the active autofill node if the session is set
        // inactive. Likewise, we should focus a node once we return.
        mainSession.loadUri(pageUrl)
        // Wait for the auto-fill nodes to populate.
        sessionRule.waitUntilCalled(object : Autofill.Delegate, GeckoSession.ProgressDelegate {
            // For the root document and the iframe document, each has a form group and
            // a group for inputs outside of forms, so the total count is 4.
            @AssertCalled(count = 1)
            override fun onSessionStart(session: GeckoSession) {}

            @AssertCalled(count = -1)
            override fun onNodeAdd(
                session: GeckoSession,
                node: Autofill.Node,
                data: Autofill.NodeData,
            ) {}

            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {}
        })

        mainSession.evaluateJS("document.querySelector('#pass2').focus()")
        sessionRule.waitUntilCalled(object : Autofill.Delegate {
            @AssertCalled(count = 1)
            override fun onNodeFocus(
                session: GeckoSession,
                node: Autofill.Node,
                data: Autofill.NodeData,
            ) {
                assertThat("ID should be valid", node, notNullValue())
            }
        })

        var focused = mainSession.autofillSession.focused
        assertThat(
            "Should have one focused field",
            countAutofillNodes({ it == focused }),
            equalTo(1),
        )

        // Make sure we get NODE_BLURRED when inactive
        mainSession.setActive(false)
        sessionRule.waitUntilCalled(object : Autofill.Delegate {
            @AssertCalled(count = 1)
            override fun onNodeBlur(
                session: GeckoSession,
                node: Autofill.Node,
                data: Autofill.NodeData,
            ) {
                assertThat("ID should be valid", node, notNullValue())
            }
        })

        // Make sure we get NODE_FOCUSED when active once again
        mainSession.setActive(true)
        sessionRule.waitUntilCalled(object : Autofill.Delegate {
            @AssertCalled(count = 1)
            override fun onNodeFocus(
                session: GeckoSession,
                node: Autofill.Node,
                data: Autofill.NodeData,
            ) {
                assertThat("ID should be valid", node, notNullValue())
            }
        })

        focused = mainSession.autofillSession.focused
        assertThat(
            "Should have one focused field",
            countAutofillNodes({ focused == it }),
            equalTo(1),
        )
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun autofillAutocompleteAttribute() {
        mainSession.loadTestPath(FORMS_AUTOCOMPLETE_HTML_PATH)
        sessionRule.waitUntilCalled(object : Autofill.Delegate, GeckoSession.ProgressDelegate {
            @AssertCalled(count = -1)
            override fun onNodeAdd(
                session: GeckoSession,
                node: Autofill.Node,
                data: Autofill.NodeData,
            ) {}

            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {}
        })

        fun checkAutofillChild(child: Autofill.Node): Int {
            var sum = 0
            for (c in child.children) {
                sum += checkAutofillChild(c!!)
            }
            if (child.hint == Autofill.Hint.NONE) {
                return sum
            }
            assertThat("Should have HTML tag", child.tag, equalTo("input"))
            return sum + 1
        }

        val root = mainSession.autofillSession.root
        // Each page has 3 nodes for autofill.
        assertThat(
            "autofill hint count",
            checkAutofillChild(root),
            equalTo(6),
        )
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun autofillWaitForKeyboard() {
        // Wait for the accessibility nodes to populate.
        mainSession.loadUri(pageUrl)
        mainSession.waitForPageStop()

        mainSession.pressKey(KeyEvent.KEYCODE_CTRL_LEFT)
        mainSession.evaluateJS("document.querySelector('#pass2').focus()")

        sessionRule.waitUntilCalled(object : Autofill.Delegate, TextInputDelegate {
            @AssertCalled(order = [2])
            override fun onNodeFocus(
                session: GeckoSession,
                node: Autofill.Node,
                data: Autofill.NodeData,
            ) {
                assertThat("ID should be valid", node, notNullValue())
            }

            @AssertCalled(order = [1])
            override fun showSoftInput(session: GeckoSession) {}
        })
    }

    @WithDisplay(width = 300, height = 1000)
    @Test
    fun autofillIframe() {
        // No way to click in x-origin frame.
        assumeThat("Not in x-origin", iframe, not(equalTo("#oop")))

        // Wait for the accessibility nodes to populate.
        mainSession.loadUri(pageUrl)
        mainSession.waitForPageStop()

        // Get non-iframe position of input element
        var screenRect = Rect()
        mainSession.evaluateJS("document.querySelector('#pass2').focus()")

        sessionRule.waitUntilCalled(object : Autofill.Delegate {
            @AssertCalled(count = 1)
            override fun onNodeFocus(
                session: GeckoSession,
                node: Autofill.Node,
                data: Autofill.NodeData,
            ) {
                screenRect = node.screenRect
            }
        })

        mainSession.evaluateJS("document.querySelector('iframe').contentDocument.querySelector('#pass2').focus()")

        sessionRule.waitUntilCalled(object : Autofill.Delegate {
            @AssertCalled(count = 1)
            override fun onNodeFocus(
                session: GeckoSession,
                node: Autofill.Node,
                data: Autofill.NodeData,
            ) {
                assertThat("ID should be valid", node, notNullValue())
                // iframe's input element should consider iframe's offset. 200 is enough offset.
                assertThat("position is valid", node.getScreenRect().top, greaterThanOrEqualTo(screenRect.top + 200))
            }
        })
    }
}
