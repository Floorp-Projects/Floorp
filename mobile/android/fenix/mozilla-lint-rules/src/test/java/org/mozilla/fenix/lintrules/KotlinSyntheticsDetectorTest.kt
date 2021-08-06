/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("UnstableApiUsage")

package org.mozilla.fenix.lintrules

import com.android.tools.lint.checks.infrastructure.LintDetectorTest
import com.android.tools.lint.checks.infrastructure.TestFiles
import com.android.tools.lint.detector.api.Detector
import com.android.tools.lint.detector.api.Issue
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.JUnit4
import org.mozilla.fenix.lintrules.KotlinSyntheticsDetector.Companion.DEPRECATED_KOTLIN_SYNTHETICS_ISSUE

@RunWith(JUnit4::class)
class KotlinSyntheticsDetectorTest : LintDetectorTest() {

    override fun getIssues(): MutableList<Issue> = mutableListOf(DEPRECATED_KOTLIN_SYNTHETICS_ISSUE)

    override fun getDetector(): Detector = KotlinSyntheticsDetector()

    @Test
    fun `GIVEN no kotlin synthetics import WHEN KotlinSyntheticsDetector runs THEN there are no errors or exceptions`() {
        val code = """
            |package example
            |
            |import org.mozilla.fenix.databinding.OnboardingTrackingProtectionBinding
            |
            |class SomeExample {
            |   fun hello() {
            |       println("World")
            |   }
            |}""".trimMargin()

        lint()
            .files(TestFiles.kt(code))
            .allowCompilationErrors()
            .allowMissingSdk(true)
            .run()
            .expectClean()
    }

    @Test
    fun `GIVEN a kotlin synthetics import WHEN KotlinSyntheticsDetector runs THEN there are no errors or exceptions`() {
        val code = """
            |package example
            |
            |import kotlinx.android.synthetic.main.layout
            |
            |class SomeExample {
            |   fun hello() {
            |       println("World")
            |   }
            |}""".trimMargin()

        val expectedReport = """
            |src/example/SomeExample.kt:3: Error: Kotlin synthetics are to be replaced with Jetpack View Binding. Please refer to https://github.com/mozilla-mobile/fenix/issues/17917 for more details. [KotlinSyntheticsDeprecation]
            |import kotlinx.android.synthetic.main.layout
            |~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
            |1 errors, 0 warnings""".trimMargin()

        lint()
            .files(TestFiles.kt(code))
            .allowMissingSdk(true)
            .run()
            .expect(expectedReport)
    }
}
