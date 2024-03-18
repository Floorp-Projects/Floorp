/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.lintrules

import com.android.tools.lint.checks.infrastructure.LintDetectorTest
import com.android.tools.lint.checks.infrastructure.TestFiles
import com.android.tools.lint.checks.infrastructure.TestMode
import com.android.tools.lint.detector.api.Detector
import com.android.tools.lint.detector.api.Issue
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.JUnit4
import org.mozilla.fenix.lintrules.LicenseCommentChecker.Companion.ValidLicenseForKotlinFiles
import org.mozilla.fenix.lintrules.LicenseDetector.Companion.ISSUE_INVALID_LICENSE_FORMAT
import org.mozilla.fenix.lintrules.LicenseDetector.Companion.ISSUE_MISSING_LICENSE

@RunWith(JUnit4::class)
class LicenseDetectorTest : LintDetectorTest() {

    override fun getIssues(): MutableList<Issue> =
        mutableListOf(ISSUE_MISSING_LICENSE, ISSUE_INVALID_LICENSE_FORMAT)

    override fun getDetector(): Detector = LicenseDetector()

    @Test
    fun `report missing license if it isn't at the top of the file`() {
        val code = """
            |package example
            |
            |class SomeExample {
            |   fun test() {
            |       val x = 10
            |       println("Hello")
            |   }
            |}""".trimMargin()

        val expectedReport = """
            |src/example/SomeExample.kt:1: Warning: The file must start with a comment containing the license [MissingLicense]
            |package example
            |~~~~~~~~~~~~~~~
            |0 errors, 1 warnings""".trimMargin()

        val expectedFixOutput = """
            |Fix for src/example/SomeExample.kt line 1: Insert license at the top of the file:
            |@@ -1 +1
            |+ /* This Source Code Form is subject to the terms of the Mozilla Public
            |+  * License, v. 2.0. If a copy of the MPL was not distributed with this
            |+  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
            |+""".trimMargin()

        lint()
            .files(TestFiles.kt(code))
            .allowMissingSdk(true)
            .testModes(TestMode.UI_INJECTION_HOST)
            .run()
            .expect(expectedReport)
            .expectFixDiffs(expectedFixOutput)
    }

    @Test
    fun `report invalid license format if it doesn't have a leading newline character`() {
        val code = """
            |$ValidLicenseForKotlinFiles
            |package example
            |
            |class SomeExample {
            |    fun test() {
            |        val x = 10
            |        println("Hello")
            |    }
            |}""".trimMargin()

        val expectedReport = """
            |src/example/SomeExample.kt:3: Warning: The license comment must be followed by a newline character [InvalidLicenseFormat]
            | * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
            |                                                             ~
            |0 errors, 1 warnings""".trimMargin()

        val expectedFixOutput = """
            |Fix for src/example/SomeExample.kt line 3: Insert newline character after the license:
            |@@ -4 +4
            |+""".trimMargin()

        lint()
            .files(TestFiles.kt(code))
            .allowMissingSdk(true)
            .testModes(TestMode.UI_INJECTION_HOST)
            .run()
            .expect(expectedReport)
            .expectFixDiffs(expectedFixOutput)
    }

    @Test
    fun `valid license does not generate a warning`() {
        val code = """
            |$ValidLicenseForKotlinFiles
            |
            |package example
            |
            |class SomeExample {
            |    fun test() {
            |        val x = 10
            |        println("Hello")
            |    }
            |}""".trimMargin()

        lint()
            .files(TestFiles.kt(code))
            .allowMissingSdk(true)
            .testModes(TestMode.UI_INJECTION_HOST)
            .run()
            .expectClean()
    }

    @Test
    fun `report incorrectly formatted license`() {
        val code = """
            |/* This Source Code Form is subject to the terms of the Mozilla Public
            |   License, v. 2.0. If a copy of the MPL was not distributed with this
            |   file, You can obtain one at http://mozilla.org/MPL/2.0/. */
            |
            |package example
            |
            |class SomeExample {
            |    fun test() {
            |        val x = 10
            |        println("Hello")
            |    }
            |}""".trimMargin()

        val expectedReport = """
            |src/example/SomeExample.kt:1: Warning: The license comment doesn't have the appropriate format [InvalidLicenseFormat]
            |/* This Source Code Form is subject to the terms of the Mozilla Public
            |^
            |0 errors, 1 warnings""".trimMargin()

        val expectedFixOutput = """
            |Fix for src/example/SomeExample.kt line 1: Replace with correctly formatted license:
            |@@ -2 +2
            |-    License, v. 2.0. If a copy of the MPL was not distributed with this
            |-    file, You can obtain one at http://mozilla.org/MPL/2.0/. */
            |+  * License, v. 2.0. If a copy of the MPL was not distributed with this
            |+  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */""".trimMargin()

        lint()
            .files(TestFiles.kt(code))
            .allowMissingSdk(true)
            .testModes(TestMode.UI_INJECTION_HOST)
            .run()
            .expect(expectedReport)
            .expectFixDiffs(expectedFixOutput)
    }
}
