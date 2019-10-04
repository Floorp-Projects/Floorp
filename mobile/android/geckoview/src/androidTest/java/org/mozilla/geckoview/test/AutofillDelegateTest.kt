/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import android.graphics.Matrix
import android.os.Bundle
import android.os.LocaleList
import android.support.test.filters.MediumTest
import android.support.test.runner.AndroidJUnit4
import android.util.Pair
import android.util.SparseArray
import android.view.View
import android.view.ViewStructure
import android.view.autofill.AutofillId
import android.view.autofill.AutofillValue
import org.hamcrest.Matchers.*
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.AutofillElement
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDisplay
import org.mozilla.geckoview.test.util.Callbacks


@RunWith(AndroidJUnit4::class)
@MediumTest
class AutofillDelegateTest : BaseSessionTest() {

    @Test fun autofill() {
        // Test parts of the Oreo auto-fill API; there is another autofill test in
        // SessionAccessibility for a11y auto-fill support.
        mainSession.loadTestPath(FORMS_HTML_PATH)
        // Wait for the auto-fill nodes to populate.
        sessionRule.waitUntilCalled(object : Callbacks.AutofillDelegate {
            // For the root document and the iframe document, each has a form group and
            // a group for inputs outside of forms, so the total count is 4.
            @AssertCalled(count = 4)
            override fun onAutofill(session: GeckoSession, notification: Int, virtualId: Int) {
            }
        })

        val autoFills = mapOf(
                "#user1" to "bar", "#user2" to "bar",
                "#pass1" to "baz", "#pass2" to "baz", "#email1" to "a@b.c",
                "#number1" to "24", "#tel1" to "42")

        // Set up promises to monitor the values changing.
        val promises = autoFills.flatMap { entry ->
            // Repeat each test with both the top document and the iframe document.
            arrayOf("document", "document.querySelector('#iframe').contentDocument").map { doc ->
                mainSession.evaluatePromiseJS("""new Promise(resolve =>
                    $doc.querySelector('${entry.key}').addEventListener(
                      'input', event => {
                        let eventInterface =
                          event instanceof InputEvent ? "InputEvent" :
                          event instanceof UIEvent ? "UIEvent" :
                          event instanceof Event ? "Event" : "Unknown";
                        resolve([
                          '${entry.key}',
                          event.target.value,
                          '${entry.value}',
                          eventInterface
                        ]);
                }, { once: true }))""")
            }
        }

        val autoFillValues = SparseArray<CharSequence>()

        // Perform auto-fill and return number of auto-fills performed.
        fun checkAutoFillChild(child: AutofillElement) {
            // Seal the node info instance so we can perform actions on it.
            if (child.children.count() > 0) {
                for (c in child.children) {
                    checkAutoFillChild(c!!)
                }
            }

            if (child.id == View.NO_ID) {
                return
            }

            assertThat("Should have HTML tag",
                       child.tag, not(isEmptyOrNullString()))
            assertThat("Web domain should match",
                       child.domain, equalTo(GeckoSessionTestRule.TEST_ENDPOINT))

            if (child.inputType == AutofillElement.INPUT_TYPE_TEXT) {
                assertThat("Input should be enabled", child.enabled, equalTo(true))
                assertThat("Input should be focusable",
                        child.focusable, equalTo(true))

                assertThat("Should have HTML tag", child.tag, equalTo("input"))
                assertThat("Should have ID attribute", child.attributes.get("id"), not(isEmptyOrNullString()))
            }

            autoFillValues.append(child.id, when (child.inputType) {
                AutofillElement.INPUT_TYPE_NUMBER -> "24"
                AutofillElement.INPUT_TYPE_PHONE -> "42"
                AutofillElement.INPUT_TYPE_TEXT -> when (child.hint) {
                    AutofillElement.HINT_PASSWORD -> "baz"
                    AutofillElement.HINT_EMAIL_ADDRESS -> "a@b.c"
                    else -> "bar"
                }
                else -> "bar"
            })
        }

        val elements = mainSession.autofillElements
        checkAutoFillChild(elements)

        mainSession.autofill(autoFillValues)

        // Wait on the promises and check for correct values.
        for ((key, actual, expected, eventInterface) in promises.map { it.value.asJSList<String>() }) {
            assertThat("Auto-filled value must match ($key)", actual, equalTo(expected))

            // <input type=number> elements don't get InputEvent events.
            if (key == "#number1") {
                assertThat("input type=number event should be dispatched with Event interface", eventInterface, equalTo("Event"))
            } else {
                assertThat("input event should be dispatched with InputEvent interface", eventInterface, equalTo("InputEvent"))
            }
        }
    }

    private fun countAutoFillNodes(cond: (AutofillElement) -> Boolean =
                                   { it.inputType != AutofillElement.INPUT_TYPE_NONE },
                           root: AutofillElement? = null): Int {
        val node = if (root !== null) root else mainSession.autofillElements
        return (if (cond(node)) 1 else 0) +
                node.children.sumBy {
                    countAutoFillNodes(cond, it) }
    }

    @WithDisplay(width = 100, height = 100)
    @Test fun autoFill_navigation() {
        // Wait for the accessibility nodes to populate.
        mainSession.loadTestPath(FORMS_HTML_PATH)
        sessionRule.waitUntilCalled(object : Callbacks.AutofillDelegate {
            @AssertCalled(count = 4)
            override fun onAutofill(session: GeckoSession, notification: Int, virtualId: Int) {
                assertThat("Should be starting auto-fill", notification, equalTo(forEachCall(
                        GeckoSession.AutofillDelegate.AUTOFILL_NOTIFY_STARTED,
                        GeckoSession.AutofillDelegate.AUTOFILL_NOTIFY_VIEW_ADDED)))
                assertThat("ID should be valid", virtualId, not(equalTo(View.NO_ID)))
            }
        })

        assertThat("Initial auto-fill count should match",
                   countAutoFillNodes(), equalTo(14))

        // Now wait for the nodes to clear.
        mainSession.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitUntilCalled(object : Callbacks.AutofillDelegate {
            @AssertCalled(count = 1)
            override fun onAutofill(session: GeckoSession, notification: Int, virtualId: Int) {
                assertThat("Should be canceling auto-fill",
                           notification,
                           equalTo(GeckoSession.AutofillDelegate.AUTOFILL_NOTIFY_CANCELED))
                assertThat("ID should be valid", virtualId, equalTo(View.NO_ID))
            }
        })
        assertThat("Should not have auto-fill fields",
                   countAutoFillNodes(), equalTo(0))

        // Now wait for the nodes to reappear.
        mainSession.waitForPageStop()
        mainSession.goBack()
        sessionRule.waitUntilCalled(object : Callbacks.AutofillDelegate {
            @AssertCalled(count = 4)
            override fun onAutofill(session: GeckoSession, notification: Int, virtualId: Int) {
                assertThat("Should be starting auto-fill", notification, equalTo(forEachCall(
                        GeckoSession.AutofillDelegate.AUTOFILL_NOTIFY_STARTED,
                        GeckoSession.AutofillDelegate.AUTOFILL_NOTIFY_VIEW_ADDED)))
                assertThat("ID should be valid", virtualId, not(equalTo(View.NO_ID)))
            }
        })
        assertThat("Should have auto-fill fields again",
                   countAutoFillNodes(), equalTo(14))
        assertThat("Should not have focused field",
                   countAutoFillNodes({ it.focused }), equalTo(0))

        mainSession.evaluateJS("document.querySelector('#pass2').focus()")
        sessionRule.waitUntilCalled(object : Callbacks.AutofillDelegate {
            @AssertCalled(count = 1)
            override fun onAutofill(session: GeckoSession, notification: Int, virtualId: Int) {
                assertThat("Should be entering auto-fill view",
                           notification,
                           equalTo(GeckoSession.AutofillDelegate.AUTOFILL_NOTIFY_VIEW_ENTERED))
                assertThat("ID should be valid", virtualId, not(equalTo(View.NO_ID)))
            }
        })
        assertThat("Should have one focused field",
                   countAutoFillNodes({ it.focused }), equalTo(1))
        // The focused field, its siblings, its parent, and the root node should be visible.
        assertThat("Should have seven visible nodes",
                   countAutoFillNodes({ node -> node.dimensions.width() > 0 && node.dimensions.height() > 0 }),
                   equalTo(7))

        mainSession.evaluateJS("document.querySelector('#pass2').blur()")
        sessionRule.waitUntilCalled(object : Callbacks.AutofillDelegate {
            @AssertCalled(count = 1)
            override fun onAutofill(session: GeckoSession, notification: Int, virtualId: Int) {
                assertThat("Should be exiting auto-fill view",
                           notification,
                           equalTo(GeckoSession.AutofillDelegate.AUTOFILL_NOTIFY_VIEW_EXITED))
                assertThat("ID should be valid", virtualId, not(equalTo(View.NO_ID)))
            }
        })
        assertThat("Should not have focused field",
                   countAutoFillNodes({ it.focused }), equalTo(0))
    }

    @WithDisplay(height = 100, width = 100)
    @Test fun autofill_userpass() {
        mainSession.loadTestPath(FORMS2_HTML_PATH)
        // Wait for the auto-fill nodes to populate.
        sessionRule.waitUntilCalled(object : Callbacks.AutofillDelegate {
            @AssertCalled(count = 3)
            override fun onAutofill(session: GeckoSession, notification: Int, virtualId: Int) {
                assertThat("Autofill notification should match", notification,
                        equalTo(forEachCall(GeckoSession.AutofillDelegate.AUTOFILL_NOTIFY_STARTED,
                                GeckoSession.AutofillDelegate.AUTOFILL_NOTIFY_VIEW_ENTERED,
                                GeckoSession.AutofillDelegate.AUTOFILL_NOTIFY_VIEW_ADDED)))
            }
        })

        // Perform auto-fill and return number of auto-fills performed.
        fun checkAutoFillChild(child: AutofillElement): Int {
            var sum = 0
            // Seal the node info instance so we can perform actions on it.
            for (c in child.children) {
                sum += checkAutoFillChild(c!!)
            }

            if (child.hint == AutofillElement.HINT_NONE) {
                return sum
            }

            assertThat("ID should be valid", child.id, not(equalTo(View.NO_ID)))
            assertThat("Should have HTML tag", child.tag, equalTo("input"))

            return sum + 1
        }

        val root = mainSession.autofillElements

        // form and iframe have each have 2 elements with hints.
        assertThat("autofill hint count",
                   checkAutoFillChild(root), equalTo(4))
    }

    @WithDisplay(width = 100, height = 100)
    @Test fun autofillActiveChange() {
        // We should blur the active autofill element if the session is set
        // inactive. Likewise, we should focus an element once we return.
        mainSession.loadTestPath(FORMS_HTML_PATH)
        // Wait for the auto-fill nodes to populate.
        sessionRule.waitUntilCalled(object : Callbacks.AutofillDelegate {
            // For the root document and the iframe document, each has a form group and
            // a group for inputs outside of forms, so the total count is 4.
            @AssertCalled(count = 4)
            override fun onAutofill(session: GeckoSession, notification: Int, virtualId: Int) {
            }
        })

        mainSession.evaluateJS("document.querySelector('#pass2').focus()")
        sessionRule.waitUntilCalled(object : Callbacks.AutofillDelegate {
            @AssertCalled(count = 1)
            override fun onAutofill(session: GeckoSession, notification: Int, virtualId: Int) {
                assertThat("Should be entering auto-fill view",
                        notification,
                        equalTo(GeckoSession.AutofillDelegate.AUTOFILL_NOTIFY_VIEW_ENTERED))
                assertThat("ID should be valid", virtualId, not(equalTo(View.NO_ID)))
            }
        })
        assertThat("Should have one focused field",
                countAutoFillNodes({ it.focused }), equalTo(1))

        // Make sure we get VIEW_EXITED when inactive
        mainSession.setActive(false)
        sessionRule.waitUntilCalled(object : Callbacks.AutofillDelegate {
            @AssertCalled(count = 1)
            override fun onAutofill(session: GeckoSession, notification: Int, virtualId: Int) {
                assertThat("Should be exiting auto-fill view",
                        notification,
                        equalTo(GeckoSession.AutofillDelegate.AUTOFILL_NOTIFY_VIEW_EXITED))
                assertThat("ID should be valid", virtualId, not(equalTo(View.NO_ID)))
            }
        })

        // Make sure we get VIEW_ENTERED when active once again
        mainSession.setActive(true)
        sessionRule.waitUntilCalled(object : Callbacks.AutofillDelegate {
            @AssertCalled(count = 1)
            override fun onAutofill(session: GeckoSession, notification: Int, virtualId: Int) {
                assertThat("Should be entering auto-fill view",
                        notification,
                        equalTo(GeckoSession.AutofillDelegate.AUTOFILL_NOTIFY_VIEW_ENTERED))
                assertThat("ID should be valid", virtualId, not(equalTo(View.NO_ID)))
            }
        })
        assertThat("Should have one focused field",
                countAutoFillNodes({ it.focused }), equalTo(1))
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
