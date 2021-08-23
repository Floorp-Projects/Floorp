/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import android.graphics.Matrix
import android.os.Bundle
import android.os.LocaleList
import androidx.test.filters.MediumTest
import androidx.test.ext.junit.runners.AndroidJUnit4
import android.util.Pair
import android.util.SparseArray
import android.view.View
import android.view.ViewStructure
import android.view.autofill.AutofillId
import android.view.autofill.AutofillValue
import org.hamcrest.Matchers.*
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.Autofill
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDisplay


@RunWith(AndroidJUnit4::class)
@MediumTest
class AutofillDelegateTest : BaseSessionTest() {

    @Test fun autofillCommit() {
        sessionRule.setPrefsUntilTestEnd(mapOf(
                "signon.rememberSignons" to true,
                "signon.userInputRequiredToCapture.enabled" to false))

        mainSession.loadTestPath(FORMS_HTML_PATH)
        // Wait for the auto-fill nodes to populate.
        sessionRule.waitUntilCalled(object : Autofill.Delegate {
            // For the root document and the iframe document, each has a form group and
            // a group for inputs outside of forms, so the total count is 4.
            @AssertCalled(count = 4)
            override fun onAutofill(session: GeckoSession,
                                    notification: Int,
                                    node: Autofill.Node?) {
                assertThat("Should be starting auto-fill",
                           notification,
                           equalTo(forEachCall(
                              Autofill.Notify.SESSION_STARTED,
                              Autofill.Notify.NODE_ADDED)))
            }
        })

        // Assign node values.
        mainSession.evaluateJS("document.querySelector('#user1').value = 'user1x'")
        mainSession.evaluateJS("document.querySelector('#pass1').value = 'pass1x'")
        mainSession.evaluateJS("document.querySelector('#email1').value = 'e@mail.com'")
        mainSession.evaluateJS("document.querySelector('#number1').value = '1'")

        // Submit the session.
        mainSession.evaluateJS("document.querySelector('#form1').submit()")

        sessionRule.waitUntilCalled(object : Autofill.Delegate {
            @AssertCalled(count = 5)
            override fun onAutofill(session: GeckoSession,
                                    notification: Int,
                                    node: Autofill.Node?) {
                val info = sessionRule.currentCall

                if (info.counter < 5) {
                    assertThat("Should be an update notification",
                               notification,
                               equalTo(Autofill.Notify.NODE_UPDATED))
                } else {
                    assertThat("Should be a commit notification",
                               notification,
                               equalTo(Autofill.Notify.SESSION_COMMITTED))

                    assertThat("Values should match",
                               countAutofillNodes({ it.value == "user1x" }),
                               equalTo(1))
                    assertThat("Values should match",
                               countAutofillNodes({ it.value == "pass1x" }),
                               equalTo(1))
                    assertThat("Values should match",
                               countAutofillNodes({ it.value == "e@mail.com" }),
                               equalTo(1))
                    assertThat("Values should match",
                               countAutofillNodes({ it.value == "1" }),
                               equalTo(1))
                }
            }
        })
    }

    @Test fun autofillCommitIdValue() {
        sessionRule.setPrefsUntilTestEnd(mapOf(
                "signon.rememberSignons" to true,
                "signon.userInputRequiredToCapture.enabled" to false))

        mainSession.loadTestPath(FORMS_ID_VALUE_HTML_PATH)
        // Wait for the auto-fill nodes to populate.
        sessionRule.waitUntilCalled(object : Autofill.Delegate {
            @AssertCalled(count = 1)
            override fun onAutofill(session: GeckoSession,
                                    notification: Int,
                                    node: Autofill.Node?) {
                assertThat("Should be starting auto-fill",
                           notification,
                           equalTo(forEachCall(
                              Autofill.Notify.SESSION_STARTED,
                              Autofill.Notify.NODE_ADDED)))
            }
        })

        // Assign node values.
        mainSession.evaluateJS("document.querySelector('#value').value = 'pass1x'")

        // Submit the session.
        mainSession.evaluateJS("document.querySelector('#form1').submit()")

        sessionRule.waitUntilCalled(object : Autofill.Delegate {
            @AssertCalled(count = 2)
            override fun onAutofill(session: GeckoSession,
                                    notification: Int,
                                    node: Autofill.Node?) {
                val info = sessionRule.currentCall

                if (info.counter < 2) {
                    assertThat("Should be an update notification",
                               notification,
                               equalTo(Autofill.Notify.NODE_UPDATED))
                } else {
                    assertThat("Should be a commit notification",
                               notification,
                               equalTo(Autofill.Notify.SESSION_COMMITTED))

                    assertThat("Values should match",
                               countAutofillNodes({ it.value == "pass1x" }),
                               equalTo(1))
                }
            }
        })
    }

    @Test fun autofill() {
        // Test parts of the Oreo auto-fill API; there is another autofill test in
        // SessionAccessibility for a11y auto-fill support.
        mainSession.loadTestPath(FORMS_HTML_PATH)
        // Wait for the auto-fill nodes to populate.
        sessionRule.waitUntilCalled(object : Autofill.Delegate {
            // For the root document and the iframe document, each has a form group and
            // a group for inputs outside of forms, so the total count is 4.
            @AssertCalled(count = 4)
            override fun onAutofill(session: GeckoSession,
                                    notification: Int,
                                    node: Autofill.Node?) {
            }
        })

        val autofills = mapOf(
                "#user1" to "bar", "#user2" to "bar",
                "#pass1" to "baz", "#pass2" to "baz", "#email1" to "a@b.c",
                "#number1" to "24", "#tel1" to "42")

        // Set up promises to monitor the values changing.
        val promises = autofills.flatMap { entry ->
            // Repeat each test with both the top document and the iframe document.
            arrayOf("document", "document.querySelector('#iframe').contentDocument").map { doc ->
                mainSession.evaluatePromiseJS("""new Promise(resolve =>
                    $doc.querySelector('${entry.key}').addEventListener(
                      'input', event => {
                        let eventInterface =
                          event instanceof $doc.defaultView.InputEvent ? "InputEvent" :
                          event instanceof $doc.defaultView.UIEvent ? "UIEvent" :
                          event instanceof $doc.defaultView.Event ? "Event" : "Unknown";
                        resolve([
                          '${entry.key}',
                          event.target.value,
                          '${entry.value}',
                          eventInterface
                        ]);
                }, { once: true }))""")
            }
        }

        val autofillValues = SparseArray<CharSequence>()

        // Perform auto-fill and return number of auto-fills performed.
        fun checkAutofillChild(child: Autofill.Node) {
            // Seal the node info instance so we can perform actions on it.
            if (child.children.count() > 0) {
                for (c in child.children) {
                    checkAutofillChild(c!!)
                }
            }

            if (child.id == View.NO_ID) {
                return
            }

            assertThat("Should have HTML tag",
                       child.tag, not(isEmptyOrNullString()))
            assertThat("Web domain should match",
                       child.domain, equalTo(GeckoSessionTestRule.TEST_ENDPOINT))

            if (child.inputType == Autofill.InputType.TEXT) {
                assertThat("Input should be enabled", child.enabled, equalTo(true))
                assertThat("Input should be focusable",
                        child.focusable, equalTo(true))

                assertThat("Should have HTML tag", child.tag, equalTo("input"))
                assertThat("Should have ID attribute", child.attributes.get("id"), not(isEmptyOrNullString()))
            }

            autofillValues.append(child.id, when (child.inputType) {
                Autofill.InputType.NUMBER -> "24"
                Autofill.InputType.PHONE -> "42"
                Autofill.InputType.TEXT -> when (child.hint) {
                    Autofill.Hint.PASSWORD -> "baz"
                    Autofill.Hint.EMAIL_ADDRESS -> "a@b.c"
                    else -> "bar"
                }
                else -> "bar"
            })
        }

        val nodes = mainSession.autofillSession.root
        checkAutofillChild(nodes)

        mainSession.autofill(autofillValues)

        // Wait on the promises and check for correct values.
        for ((key, actual, expected, eventInterface) in promises.map { it.value.asJSList<String>() }) {
            assertThat("Auto-filled value must match ($key)", actual, equalTo(expected))
            assertThat("input event should be dispatched with InputEvent interface", eventInterface, equalTo("InputEvent"))
        }
    }

    private fun countAutofillNodes(cond: (Autofill.Node) -> Boolean =
                                   { it.inputType != Autofill.InputType.NONE },
                           root: Autofill.Node? = null): Int {
        val node = if (root !== null) root else mainSession.autofillSession.root
        return (if (cond(node)) 1 else 0) +
                node.children.sumBy {
                    countAutofillNodes(cond, it) }
    }

    @WithDisplay(width = 100, height = 100)
    @Test fun autofillNavigation() {
        // Wait for the accessibility nodes to populate.
        mainSession.loadTestPath(FORMS_HTML_PATH)
        sessionRule.waitUntilCalled(object : Autofill.Delegate {
            @AssertCalled(count = 4)
            override fun onAutofill(session: GeckoSession,
                                    notification: Int,
                                    node: Autofill.Node?) {
                assertThat("Should be starting auto-fill",
                           notification,
                           equalTo(forEachCall(
                              Autofill.Notify.SESSION_STARTED,
                              Autofill.Notify.NODE_ADDED)))
                assertThat("Node should be valid", node, notNullValue())
            }
        })

        assertThat("Initial auto-fill count should match",
                   countAutofillNodes(), equalTo(16))

        // Now wait for the nodes to clear.
        mainSession.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitUntilCalled(object : Autofill.Delegate {
            @AssertCalled(count = 1)
            override fun onAutofill(session: GeckoSession,
                                    notification: Int,
                                    node: Autofill.Node?) {
                assertThat("Should be canceling auto-fill",
                           notification,
                           equalTo(Autofill.Notify.SESSION_CANCELED))
                assertThat("Node should be null", node, nullValue())
            }
        })
        assertThat("Should not have auto-fill fields",
                   countAutofillNodes(), equalTo(0))

        // Now wait for the nodes to reappear.
        mainSession.waitForPageStop()
        mainSession.goBack()
        sessionRule.waitUntilCalled(object : Autofill.Delegate {
            @AssertCalled(count = 4)
            override fun onAutofill(session: GeckoSession,
                                    notification: Int,
                                    node: Autofill.Node?) {
                assertThat("Should be starting auto-fill",
                           notification,
                           equalTo(forEachCall(
                               Autofill.Notify.SESSION_STARTED,
                               Autofill.Notify.NODE_ADDED)))
                assertThat("ID should be valid", node, notNullValue())
            }
        })
        assertThat("Should have auto-fill fields again",
                   countAutofillNodes(), equalTo(16))
        assertThat("Should not have focused field",
                   countAutofillNodes({ it.focused }), equalTo(0))

        mainSession.evaluateJS("document.querySelector('#pass2').focus()")

        sessionRule.waitUntilCalled(object : Autofill.Delegate {
            @AssertCalled(count = 1)
            override fun onAutofill(session: GeckoSession,
                                    notification: Int,
                                    node: Autofill.Node?) {
                assertThat("Should be entering auto-fill view",
                           notification,
                           equalTo(Autofill.Notify.NODE_FOCUSED))
                assertThat("ID should be valid", node, notNullValue())
            }
        })
        assertThat("Should have one focused field",
                   countAutofillNodes({ it.focused }), equalTo(1))
        // The focused field, its siblings, its parent, and the root node should
        // be visible.
        // Hidden elements are ignored.
        // TODO: Is this actually correct? Should the whole focused branch be
        // visible or just the nodes as described above?
        assertThat("Should have nine visible nodes",
                   countAutofillNodes({ node -> node.visible }),
                   equalTo(8))

        mainSession.evaluateJS("document.querySelector('#pass2').blur()")
        sessionRule.waitUntilCalled(object : Autofill.Delegate {
            @AssertCalled(count = 1)
            override fun onAutofill(session: GeckoSession,
                                    notification: Int,
                                    node: Autofill.Node?) {
                assertThat("Should be exiting auto-fill view",
                           notification,
                           equalTo(Autofill.Notify.NODE_BLURRED))
                assertThat("ID should be valid", node, notNullValue())
            }
        })
        assertThat("Should not have focused field",
                   countAutofillNodes({ it.focused }), equalTo(0))
    }

    @WithDisplay(height = 100, width = 100)
    @Test fun autofillUserpass() {
        mainSession.loadTestPath(FORMS2_HTML_PATH)
        // Wait for the auto-fill nodes to populate.
        sessionRule.waitUntilCalled(object : Autofill.Delegate {
            @AssertCalled(count = 3)
            override fun onAutofill(session: GeckoSession,
                                    notification: Int,
                                    node: Autofill.Node?) {
                assertThat("Autofill notification should match", notification,
                        equalTo(forEachCall(Autofill.Notify.SESSION_STARTED,
                                Autofill.Notify.NODE_FOCUSED,
                                Autofill.Notify.NODE_ADDED)))
            }
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

            assertThat("ID should be valid", child.id, not(equalTo(View.NO_ID)))
            assertThat("Should have HTML tag", child.tag, equalTo("input"))

            return sum + 1
        }

        val root = mainSession.autofillSession.root

        // form and iframe have each have 2 nodes with hints.
        assertThat("autofill hint count",
                   checkAutofillChild(root), equalTo(4))
    }

    @WithDisplay(width = 100, height = 100)
    @Test fun autofillActiveChange() {
        // We should blur the active autofill node if the session is set
        // inactive. Likewise, we should focus a node once we return.
        mainSession.loadTestPath(FORMS_HTML_PATH)
        // Wait for the auto-fill nodes to populate.
        sessionRule.waitUntilCalled(object : Autofill.Delegate {
            // For the root document and the iframe document, each has a form group and
            // a group for inputs outside of forms, so the total count is 4.
            @AssertCalled(count = 4)
            override fun onAutofill(session: GeckoSession,
                                    notification: Int,
                                    node: Autofill.Node?) {
                assertThat("Should be starting auto-fill",
                           notification,
                           equalTo(forEachCall(
                              Autofill.Notify.SESSION_STARTED,
                              Autofill.Notify.NODE_ADDED)))
            }
        })

        mainSession.evaluateJS("document.querySelector('#pass2').focus()")
        sessionRule.waitUntilCalled(object : Autofill.Delegate {
            @AssertCalled(count = 1)
            override fun onAutofill(session: GeckoSession,
                                    notification: Int,
                                    node: Autofill.Node?) {
                assertThat("Should be entering auto-fill view",
                        notification,
                        equalTo(Autofill.Notify.NODE_FOCUSED))
                assertThat("ID should be valid", node, notNullValue())
            }
        })
        assertThat("Should have one focused field",
                countAutofillNodes({ it.focused }), equalTo(1))

        // Make sure we get NODE_BLURRED when inactive
        mainSession.setActive(false)
        sessionRule.waitUntilCalled(object : Autofill.Delegate {
            @AssertCalled(count = 1)
            override fun onAutofill(session: GeckoSession,
                                    notification: Int,
                                    node: Autofill.Node?) {
                assertThat("Should be exiting auto-fill view",
                        notification,
                        equalTo(Autofill.Notify.NODE_BLURRED))
                assertThat("ID should be valid", node, notNullValue())
            }
        })

        // Make sure we get NODE_FOCUSED when active once again
        mainSession.setActive(true)
        sessionRule.waitUntilCalled(object : Autofill.Delegate {
            @AssertCalled(count = 1)
            override fun onAutofill(session: GeckoSession,
                                    notification: Int,
                                    node: Autofill.Node?) {
                assertThat("Should be entering auto-fill view",
                        notification,
                        equalTo(Autofill.Notify.NODE_FOCUSED))
                assertThat("ID should be valid", node, notNullValue())
            }
        })
        assertThat("Should have one focused field",
                countAutofillNodes({ it.focused }), equalTo(1))
    }

    @WithDisplay(width = 100, height = 100)
    @Test fun autofillAutocompleteAttribute() {
        mainSession.loadTestPath(FORMS_AUTOCOMPLETE_HTML_PATH)
        sessionRule.waitUntilCalled(object : Autofill.Delegate {
            @AssertCalled(count = 3)
            override fun onAutofill(session: GeckoSession,
                                    notification: Int,
                                    node: Autofill.Node?) {
            }
        });

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
        assertThat("autofill hint count",
                   checkAutofillChild(root), equalTo(6))
    }

    class MockViewNode : ViewStructure() {
        private var mClassName: String? = null
        private var mEnabled = false
        private var mVisibility = -1
        private var mPackageName: String? = null
        private var mTypeName: String? = null
        private var mEntryName: String? = null
        private var mAutofillType = -1
        private var mAutofillHints: Array<String>? = null
        private var mInputType = -1
        private var mHtmlInfo: HtmlInfo? = null
        private var mWebDomain: String? = null
        private var mFocused = false
        private var mFocusable = false

        var children = ArrayList<MockViewNode?>()
        var id = View.NO_ID
        var height = 0
        var width = 0

        val className get() = mClassName
        val htmlInfo get() = mHtmlInfo
        val autofillHints get() = mAutofillHints
        val autofillType get() = mAutofillType
        val webDomain get() = mWebDomain
        val isEnabled get() = mEnabled
        val isFocused get() = mFocused
        val isFocusable get() = mFocusable
        val visibility get() = mVisibility
        val inputType get() = mInputType

        override fun setId(id: Int, packageName: String?, typeName: String?, entryName: String?) {
            this.id = id
            mPackageName = packageName
            mTypeName = typeName
            mEntryName = entryName
        }

        override fun setHint(hint: CharSequence?) {
            TODO("not implemented")
        }

        override fun setElevation(elevation: Float) {
            TODO("not implemented")
        }

        override fun getText(): CharSequence {
            TODO("not implemented")
        }

        override fun setText(text: CharSequence?) {
            TODO("not implemented")
        }

        override fun setText(text: CharSequence?, selectionStart: Int, selectionEnd: Int) {
            TODO("not implemented")
        }

        override fun asyncCommit() {
            TODO("not implemented")
        }

        override fun getChildCount(): Int = children.size

        override fun setEnabled(state: Boolean) {
            mEnabled = state
        }

        override fun setLocaleList(localeList: LocaleList?) {
            TODO("not implemented")
        }

        override fun setDimens(left: Int, top: Int, scrollX: Int, scrollY: Int, width: Int, height: Int) {
            this.width = width
            this.height = height
        }

        override fun setChecked(state: Boolean) {
            TODO("not implemented")
        }

        override fun setContextClickable(state: Boolean) {
            TODO("not implemented")
        }

        override fun setAccessibilityFocused(state: Boolean) {
            TODO("not implemented")
        }

        override fun setAlpha(alpha: Float) {
            TODO("not implemented")
        }

        override fun setTransformation(matrix: Matrix?) {
            TODO("not implemented")
        }

        override fun setClassName(className: String?) {
            mClassName = className
        }

        override fun setLongClickable(state: Boolean) {
            TODO("not implemented")
        }

        override fun newChild(index: Int): ViewStructure {
            val child = MockViewNode()
            children[index] = child
            return child
        }

        override fun getHint(): CharSequence {
            TODO("not implemented")
        }

        override fun setInputType(inputType: Int) {
            mInputType = inputType
        }

        override fun setWebDomain(domain: String?) {
            mWebDomain = domain
        }

        override fun setAutofillOptions(options: Array<out CharSequence>?) {
            TODO("not implemented")
        }

        override fun setTextStyle(size: Float, fgColor: Int, bgColor: Int, style: Int) {
            TODO("not implemented")
        }

        override fun setVisibility(visibility: Int) {
            mVisibility = visibility
        }

        override fun getAutofillId(): AutofillId? {
            TODO("not implemented")
        }

        override fun setHtmlInfo(htmlInfo: HtmlInfo) {
            mHtmlInfo = htmlInfo
        }

        override fun setTextLines(charOffsets: IntArray?, baselines: IntArray?) {
            TODO("not implemented")
        }

        override fun getExtras(): Bundle {
            TODO("not implemented")
        }

        override fun setClickable(state: Boolean) {
            TODO("not implemented")
        }

        override fun newHtmlInfoBuilder(tagName: String): HtmlInfo.Builder {
            return MockHtmlInfoBuilder(tagName)
        }

        override fun getTextSelectionEnd(): Int {
            TODO("not implemented")
        }

        override fun setAutofillId(id: AutofillId) {
            TODO("not implemented")
        }

        override fun setAutofillId(parentId: AutofillId, virtualId: Int) {
            TODO("not implemented")
        }

        override fun hasExtras(): Boolean {
            TODO("not implemented")
        }

        override fun addChildCount(num: Int): Int {
            TODO("not implemented")
        }

        override fun setAutofillType(type: Int) {
            mAutofillType = type
        }

        override fun setActivated(state: Boolean) {
            TODO("not implemented")
        }

        override fun setFocused(state: Boolean) {
            mFocused = state
        }

        override fun getTextSelectionStart(): Int {
            TODO("not implemented")
        }

        override fun setChildCount(num: Int) {
            children = ArrayList()
            for (i in 0 until num) {
                children.add(null)
            }
        }

        override fun setAutofillValue(value: AutofillValue?) {
            TODO("not implemented")
        }

        override fun setAutofillHints(hint: Array<String>?) {
            mAutofillHints = hint
        }

        override fun setContentDescription(contentDescription: CharSequence?) {
            TODO("not implemented")
        }

        override fun setFocusable(state: Boolean) {
            mFocusable = state
        }

        override fun setCheckable(state: Boolean) {
            TODO("not implemented")
        }

        override fun asyncNewChild(index: Int): ViewStructure {
            TODO("not implemented")
        }

        override fun setSelected(state: Boolean) {
            TODO("not implemented")
        }

        override fun setDataIsSensitive(sensitive: Boolean) {
            TODO("not implemented")
        }

        override fun setOpaque(opaque: Boolean) {
            TODO("not implemented")
        }
    }

    class MockHtmlInfoBuilder(tagName: String) : ViewStructure.HtmlInfo.Builder() {
        val mTagName = tagName
        val mAttributes: MutableList<Pair<String, String>> = mutableListOf()

        override fun addAttribute(name: String, value: String): ViewStructure.HtmlInfo.Builder {
            mAttributes.add(Pair(name, value))
            return this
        }

        override fun build(): ViewStructure.HtmlInfo {
            return MockHtmlInfo(mTagName, mAttributes)
        }
    }

    class MockHtmlInfo(tagName: String, attributes: MutableList<Pair<String, String>>)
            : ViewStructure.HtmlInfo() {
        private val mTagName = tagName
        private val mAttributes = attributes

        override fun getTag() = mTagName
        override fun getAttributes() = mAttributes
    }
}
