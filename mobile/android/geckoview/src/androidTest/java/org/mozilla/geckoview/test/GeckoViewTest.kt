package org.mozilla.geckoview.test

import android.content.Context
import android.graphics.Matrix
import android.os.Build
import android.os.Bundle
import android.os.LocaleList
import android.util.Pair
import android.util.SparseArray
import android.view.View
import android.view.ViewStructure
import android.view.autofill.AutofillId
import android.view.autofill.AutofillValue
import androidx.core.view.ViewCompat
import androidx.test.ext.junit.rules.ActivityScenarioRule
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.LargeTest
import androidx.test.filters.SdkSuppress
import org.hamcrest.Matchers.equalTo
import org.junit.* // ktlint-disable no-wildcard-imports
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeThat
import org.junit.rules.RuleChain
import org.junit.runner.RunWith
import org.mozilla.geckoview.Autofill
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoView
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.NullDelegate
import org.mozilla.geckoview.test.util.UiThreadUtils
import java.io.File

@RunWith(AndroidJUnit4::class)
@LargeTest
class GeckoViewTest : BaseSessionTest() {
    val activityRule = ActivityScenarioRule(GeckoViewTestActivity::class.java)

    @get:Rule
    override val rules = RuleChain.outerRule(activityRule).around(sessionRule)

    @Before
    fun setup() {
        activityRule.scenario.onActivity {
            // Attach the default session from the session rule to the GeckoView
            it.view.setSession(sessionRule.session)
        }
    }

    @After
    fun cleanup() {
        activityRule.scenario.onActivity {
            it.view.releaseSession()
        }
    }

    @Test
    fun setSessionOnClosed() {
        activityRule.scenario.onActivity {
            it.view.session!!.close()
            it.view.setSession(GeckoSession())
        }
    }

    @Test
    fun setSessionOnOpenDoesNotThrow() {
        activityRule.scenario.onActivity {
            assertThat("Session is open", it.view.session!!.isOpen, equalTo(true))
            val newSession = GeckoSession()
            it.view.setSession(newSession)
            assertThat(
                "The new session should be correctly set.",
                it.view.session,
                equalTo(newSession),
            )
        }
    }

    @Test(expected = java.lang.IllegalStateException::class)
    fun displayAlreadyAcquired() {
        activityRule.scenario.onActivity {
            assertThat(
                "View should be attached",
                ViewCompat.isAttachedToWindow(it.view),
                equalTo(true),
            )
            it.view.session!!.acquireDisplay()
        }
    }

    @Test
    fun relaseOnDetach() {
        activityRule.scenario.onActivity {
            // The GeckoDisplay should be released when the View is detached from the window...
            it.view.onDetachedFromWindow()
            it.view.session!!.releaseDisplay(it.view.session!!.acquireDisplay())
        }
    }

    private fun waitUntilContentProcessPriority(high: List<GeckoSession>, low: List<GeckoSession>) {
        val highPids = high.map { sessionRule.getSessionPid(it) }.toSet()
        val lowPids = low.map { sessionRule.getSessionPid(it) }.toSet()

        UiThreadUtils.waitForCondition({
            val shouldBeHighPri = getContentProcessesOomScore(highPids)
            val shouldBeLowPri = getContentProcessesOomScore(lowPids)
            // Note that higher oom score means less priority
            shouldBeHighPri.count { it > 100 } == 0 &&
                shouldBeLowPri.count { it < 300 } == 0
        }, env.defaultTimeoutMillis)
    }

    fun getContentProcessesOomScore(pids: Collection<Int>): List<Int> {
        return pids.map { pid ->
            File("/proc/$pid/oom_score").readText(Charsets.UTF_8).trim().toInt()
        }
    }

    fun setupPriorityTest(): GeckoSession {
        // This makes the test a little bit faster
        sessionRule.setPrefsUntilTestEnd(
            mapOf(
                "dom.ipc.processPriorityManager.backgroundGracePeriodMS" to 0,
                "dom.ipc.processPriorityManager.backgroundPerceivableGracePeriodMS" to 0,
            ),
        )

        val otherSession = sessionRule.createOpenSession()
        // The process manager sets newly created processes to FOREGROUND priority until they
        // are de-prioritized, so we need to activate and deactivate the session to trigger
        // a setPriority call.
        otherSession.setActive(true)
        otherSession.setActive(false)

        // Need a dummy page to be able to get the PID from the session
        otherSession.loadUri("https://example.com")
        otherSession.waitForPageStop()

        mainSession.loadTestPath(HELLO_HTML_PATH)
        mainSession.waitForPageStop()

        waitUntilContentProcessPriority(
            high = listOf(mainSession),
            low = listOf(otherSession),
        )

        return otherSession
    }

    @Test
    @NullDelegate(Autofill.Delegate::class)
    fun setTabActiveKeepsTabAtHighPriority() {
        // Bug 1768102 - Doesn't seem to work on Fission
        assumeThat(env.isFission || env.isIsolatedProcess, equalTo(false))
        activityRule.scenario.onActivity {
            val otherSession = setupPriorityTest()

            // A tab with priority hint does not get de-prioritized even when
            // the surface is destroyed
            mainSession.setPriorityHint(GeckoSession.PRIORITY_HIGH)

            // This will destroy mainSession's surface and create a surface for otherSession
            it.view.setSession(otherSession)

            waitUntilContentProcessPriority(high = listOf(mainSession, otherSession), low = listOf())

            // Destroying otherSession's surface should leave mainSession as the sole high priority
            // tab
            it.view.releaseSession()

            waitUntilContentProcessPriority(high = listOf(mainSession), low = listOf())

            // Cleanup
            mainSession.setPriorityHint(GeckoSession.PRIORITY_DEFAULT)
        }
    }

    @Test
    @NullDelegate(Autofill.Delegate::class)
    fun processPriorityTest() {
        // Doesn't seem to work on Fission
        assumeThat(env.isFission || env.isIsolatedProcess, equalTo(false))
        activityRule.scenario.onActivity {
            val otherSession = setupPriorityTest()

            // After setting otherSession to the view, otherSession should be high priority
            // and mainSession should be de-prioritized
            it.view.setSession(otherSession)

            waitUntilContentProcessPriority(
                high = listOf(otherSession),
                low = listOf(mainSession),
            )

            // After releasing otherSession, both sessions should be low priority
            it.view.releaseSession()

            waitUntilContentProcessPriority(
                high = listOf(),
                low = listOf(mainSession, otherSession),
            )

            // Test that re-setting mainSession in the view raises the priority again
            it.view.setSession(mainSession)
            waitUntilContentProcessPriority(
                high = listOf(mainSession),
                low = listOf(otherSession),
            )

            // Setting the session to active should also raise priority
            otherSession.setActive(true)
            waitUntilContentProcessPriority(
                high = listOf(mainSession, otherSession),
                low = listOf(),
            )
        }
    }

    @Test
    @NullDelegate(Autofill.Delegate::class)
    fun setPriorityHint() {
        // Bug 1768102 - Doesn't seem to work on Fission
        assumeThat(env.isFission || env.isIsolatedProcess, equalTo(false))

        val otherSession = setupPriorityTest()

        // Setting priorityHint to PRIORITY_HIGH raises priority
        otherSession.setPriorityHint(GeckoSession.PRIORITY_HIGH)

        waitUntilContentProcessPriority(
            high = listOf(mainSession, otherSession),
            low = listOf(),
        )

        // Setting priorityHint to PRIORITY_DEFAULT should lower priority
        otherSession.setPriorityHint(GeckoSession.PRIORITY_DEFAULT)

        waitUntilContentProcessPriority(
            high = listOf(mainSession),
            low = listOf(otherSession),
        )
    }

    private fun visit(node: MockViewStructure, callback: (MockViewStructure) -> Unit) {
        callback(node)

        for (child in node.children) {
            if (child != null) {
                visit(child, callback)
            }
        }
    }

    @Test
    @NullDelegate(Autofill.Delegate::class)
    @SdkSuppress(minSdkVersion = Build.VERSION_CODES.O)
    fun autofillWithNoSession() {
        mainSession.loadTestPath(FORMS_XORIGIN_HTML_PATH)
        mainSession.waitForPageStop()

        val autofills = mapOf(
            "#user1" to "username@example.com",
            "#user2" to "username@example.com",
            "#pass1" to "test-password",
            "#pass2" to "test-password",
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

        activityRule.scenario.onActivity {
            val root = MockViewStructure(View.NO_ID)
            it.view.onProvideAutofillVirtualStructure(root, 0)

            val data = SparseArray<AutofillValue>()
            visit(root) { node ->
                if (node.hints?.indexOf(View.AUTOFILL_HINT_USERNAME) != -1) {
                    data.set(node.id, AutofillValue.forText("username@example.com"))
                } else if (node.hints?.indexOf(View.AUTOFILL_HINT_PASSWORD) != -1) {
                    data.set(node.id, AutofillValue.forText("test-password"))
                }
            }

            // Releasing the session will set mSession in GeckoView to null
            // this test verifies that we can still autofill correctly even in released state
            val session = it.view.releaseSession()!!
            it.view.autofill(data)

            // Put back the session and verifies that the autofill went through anyway
            it.view.setSession(session)

            // Wait on the promises and check for correct values.
            for (values in promises.map { p -> p.value.asJsonArray() }) {
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
    }

    @Test
    @NullDelegate(Autofill.Delegate::class)
    fun activityContextDelegate() {
        var delegateCalled = false
        activityRule.scenario.onActivity {
            class TestActivityDelegate : GeckoView.ActivityContextDelegate {
                override fun getActivityContext(): Context {
                    delegateCalled = true
                    return it
                }
            }
            // Set view delegate
            it.view.activityContextDelegate = TestActivityDelegate()
            val context = it.view.activityContextDelegate?.activityContext
            assertTrue("The activity context delegate was called.", delegateCalled)
            assertTrue("The activity context delegate provided the expected context.", context == it)
        }
    }

    class MockViewStructure(var id: Int, var parent: MockViewStructure? = null) : ViewStructure() {
        private var enabled: Boolean = false
        private var inputType = 0
        var children = Array<MockViewStructure?>(0, { null })
        var childIndex = 0
        var hints: Array<out String>? = null

        override fun setId(p0: Int, p1: String?, p2: String?, p3: String?) {
            id = p0
        }

        override fun setEnabled(p0: Boolean) {
            enabled = p0
        }

        override fun setChildCount(p0: Int) {
            children = Array(p0, { null })
        }

        override fun getChildCount(): Int {
            return children.size
        }

        override fun newChild(p0: Int): ViewStructure {
            val child = MockViewStructure(p0, this)
            children[childIndex++] = child
            return child
        }

        override fun asyncNewChild(p0: Int): ViewStructure {
            return newChild(p0)
        }

        override fun setInputType(p0: Int) {
            inputType = p0
        }

        fun getInputType(): Int {
            return inputType
        }

        override fun setAutofillHints(p0: Array<out String>?) {
            hints = p0
        }

        override fun addChildCount(p0: Int): Int {
            TODO()
        }

        override fun setDimens(p0: Int, p1: Int, p2: Int, p3: Int, p4: Int, p5: Int) {}
        override fun setTransformation(p0: Matrix?) {}
        override fun setElevation(p0: Float) {}
        override fun setAlpha(p0: Float) {}
        override fun setVisibility(p0: Int) {}
        override fun setClickable(p0: Boolean) {}
        override fun setLongClickable(p0: Boolean) {}
        override fun setContextClickable(p0: Boolean) {}
        override fun setFocusable(p0: Boolean) {}
        override fun setFocused(p0: Boolean) {}
        override fun setAccessibilityFocused(p0: Boolean) {}
        override fun setCheckable(p0: Boolean) {}
        override fun setChecked(p0: Boolean) {}
        override fun setSelected(p0: Boolean) {}
        override fun setActivated(p0: Boolean) {}
        override fun setOpaque(p0: Boolean) {}
        override fun setClassName(p0: String?) {}
        override fun setContentDescription(p0: CharSequence?) {}
        override fun setText(p0: CharSequence?) {}
        override fun setText(p0: CharSequence?, p1: Int, p2: Int) {}
        override fun setTextStyle(p0: Float, p1: Int, p2: Int, p3: Int) {}
        override fun setTextLines(p0: IntArray?, p1: IntArray?) {}
        override fun setHint(p0: CharSequence?) {}
        override fun getText(): CharSequence {
            return ""
        }
        override fun getTextSelectionStart(): Int {
            return 0
        }
        override fun getTextSelectionEnd(): Int {
            return 0
        }
        override fun getHint(): CharSequence {
            return ""
        }
        override fun getExtras(): Bundle {
            return Bundle()
        }
        override fun hasExtras(): Boolean {
            return false
        }

        override fun getAutofillId(): AutofillId? {
            return null
        }
        override fun setAutofillId(p0: AutofillId) {}
        override fun setAutofillId(p0: AutofillId, p1: Int) {}
        override fun setAutofillType(p0: Int) {}
        override fun setAutofillValue(p0: AutofillValue?) {}
        override fun setAutofillOptions(p0: Array<out CharSequence>?) {}
        override fun setDataIsSensitive(p0: Boolean) {}
        override fun asyncCommit() {}
        override fun setWebDomain(p0: String?) {}
        override fun setLocaleList(p0: LocaleList?) {}

        override fun newHtmlInfoBuilder(p0: String): HtmlInfo.Builder {
            return MockHtmlInfoBuilder()
        }
        override fun setHtmlInfo(p0: HtmlInfo) {
        }
    }

    class MockHtmlInfoBuilder : ViewStructure.HtmlInfo.Builder() {
        override fun addAttribute(p0: String, p1: String): ViewStructure.HtmlInfo.Builder {
            return this
        }

        override fun build(): ViewStructure.HtmlInfo {
            return MockHtmlInfo()
        }
    }

    class MockHtmlInfo : ViewStructure.HtmlInfo() {
        override fun getTag(): String {
            TODO("Not yet implemented")
        }

        override fun getAttributes(): MutableList<Pair<String, String>>? {
            TODO("Not yet implemented")
        }
    }
}
