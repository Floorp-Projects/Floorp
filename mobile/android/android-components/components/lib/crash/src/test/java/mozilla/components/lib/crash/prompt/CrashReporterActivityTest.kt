/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.prompt

import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.view.View
import android.widget.Button
import android.widget.CheckBox
import android.widget.TextView
import androidx.test.core.app.ActivityScenario
import androidx.test.core.app.ActivityScenario.launch
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.advanceUntilIdle
import mozilla.components.lib.crash.Crash
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.lib.crash.prompt.CrashReporterActivity.Companion.PREFERENCE_KEY_SEND_REPORT
import mozilla.components.lib.crash.prompt.CrashReporterActivity.Companion.SHARED_PREFERENCES_NAME
import mozilla.components.lib.crash.service.CrashReporterService
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mockito.Mockito.verify
import org.mockito.MockitoAnnotations.openMocks
import kotlin.coroutines.CoroutineContext

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class CrashReporterActivityTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val scope = coroutinesTestRule.scope

    @Mock
    lateinit var service: CrashReporterService

    @Before
    fun setUp() {
        openMocks(this)
    }

    @Test
    fun `Pressing close button sends report`() = runTestOnMain {
        CrashReporter(
            context = testContext,
            shouldPrompt = CrashReporter.Prompt.ALWAYS,
            services = listOf(service),
            scope = scope,
        ).install(testContext)

        val crash = Crash.UncaughtExceptionCrash(0, RuntimeException("Hello World"), arrayListOf())
        val scenario = coroutineContext.launchActivityWithCrash(crash)

        scenario.onActivity { activity ->
            // When
            activity.closeButton.performClick()
        }

        // Await for all coroutines to be finished
        advanceUntilIdle()

        // Then
        verify(service).report(crash)
    }

    @Test
    fun `Pressing restart button sends report`() = runTestOnMain {
        CrashReporter(
            context = testContext,
            shouldPrompt = CrashReporter.Prompt.ALWAYS,
            services = listOf(service),
            scope = scope,
        ).install(testContext)

        val crash = Crash.UncaughtExceptionCrash(0, RuntimeException("Hello World"), arrayListOf())
        val scenario = coroutineContext.launchActivityWithCrash(crash)

        scenario.onActivity { activity ->
            // When
            activity.restartButton.performClick()
        }

        // Await for all coroutines to be finished
        advanceUntilIdle()

        // Then
        verify(service).report(crash)
    }

    @Test
    fun `Custom message is set on CrashReporterActivity`() = runTestOnMain {
        CrashReporter(
            context = testContext,
            shouldPrompt = CrashReporter.Prompt.ALWAYS,
            promptConfiguration = CrashReporter.PromptConfiguration(
                message = "Hello World!",
                theme = android.R.style.Theme_DeviceDefault, // Yolo!
            ),
            services = listOf(mock()),
        ).install(testContext)

        val crash = Crash.UncaughtExceptionCrash(0, RuntimeException("Hello World"), arrayListOf())
        val scenario = coroutineContext.launchActivityWithCrash(crash)

        scenario.onActivity { activity ->
            // Then
            assertEquals("Hello World!", activity.messageView.text)
        }
    }

    @Test
    fun `Sending crash report saves checkbox state`() = runTestOnMain {
        CrashReporter(
            context = testContext,
            shouldPrompt = CrashReporter.Prompt.ALWAYS,
            services = listOf(service),
            scope = scope,
        ).install(testContext)

        val crash = Crash.UncaughtExceptionCrash(0, RuntimeException("Hello World"), arrayListOf())
        val scenario = coroutineContext.launchActivityWithCrash(crash)

        scenario.onActivity { activity ->
            // When
            activity.sendCheckbox.isChecked = true

            // Then
            assertFalse(activity.isSendReportPreferenceEnabled)

            // When
            activity.restartButton.performClick()

            // Then
            assertTrue(activity.isSendReportPreferenceEnabled)
        }
    }

    @Test
    fun `Restart button visible for main process crash`() = runTestOnMain {
        CrashReporter(
            context = testContext,
            shouldPrompt = CrashReporter.Prompt.ALWAYS,
            services = listOf(service),
            scope = scope,
        ).install(testContext)

        val crash = Crash.NativeCodeCrash(
            0,
            "",
            true,
            "",
            Crash.NativeCodeCrash.PROCESS_TYPE_MAIN,
            arrayListOf(),
        )
        val scenario = coroutineContext.launchActivityWithCrash(crash)

        scenario.onActivity { activity ->
            assertEquals(activity.restartButton.visibility, View.VISIBLE)
        }
    }

    @Test
    fun `Restart button hidden for background child process crash`() = runTestOnMain {
        CrashReporter(
            context = testContext,
            shouldPrompt = CrashReporter.Prompt.ALWAYS,
            services = listOf(service),
            scope = scope,
        ).install(testContext)

        val crash = Crash.NativeCodeCrash(
            0,
            "",
            true,
            "",
            Crash.NativeCodeCrash.PROCESS_TYPE_BACKGROUND_CHILD,
            arrayListOf(),
        )
        val scenario = coroutineContext.launchActivityWithCrash(crash)

        scenario.onActivity { activity ->
            assertEquals(activity.restartButton.visibility, View.GONE)
        }
    }

    @Test
    fun `WHEN crash is native AND background child THEN is background returns true`() = runTestOnMain {
        CrashReporter(
            context = testContext,
            shouldPrompt = CrashReporter.Prompt.ALWAYS,
            services = listOf(service),
            scope = scope,
        ).install(testContext)

        val crash = Crash.NativeCodeCrash(
            123,
            "",
            true,
            "",
            Crash.NativeCodeCrash.PROCESS_TYPE_BACKGROUND_CHILD,
            arrayListOf(),
        )

        val scenario = coroutineContext.launchActivityWithCrash(crash)

        scenario.onActivity { activity ->
            assert(activity.isRecoverableBackgroundCrash(crash))
        }
    }
}

/**
 * Launch activity scenario for certain [crash].
 */
@ExperimentalCoroutinesApi
private fun CoroutineContext.launchActivityWithCrash(
    crash: Crash,
): ActivityScenario<CrashReporterActivity> = run {
    val intent = Intent(testContext, CrashReporterActivity::class.java)
        .also { crash.fillIn(it) }

    launch<CrashReporterActivity>(intent).apply {
        onActivity { activity ->
            activity.reporterCoroutineContext = this@run
        }
    }
}

// Views
private val CrashReporterActivity.closeButton: Button get() = binding.closeButton
private val CrashReporterActivity.restartButton: Button get() = binding.restartButton
private val CrashReporterActivity.messageView: TextView get() = binding.messageView
private val CrashReporterActivity.sendCheckbox: CheckBox get() = binding.sendCheckbox

// Preferences
private val CrashReporterActivity.preferences: SharedPreferences
    get() = getSharedPreferences(SHARED_PREFERENCES_NAME, Context.MODE_PRIVATE)
private val CrashReporterActivity.isSendReportPreferenceEnabled: Boolean
    get() = preferences.getBoolean(PREFERENCE_KEY_SEND_REPORT, false)
