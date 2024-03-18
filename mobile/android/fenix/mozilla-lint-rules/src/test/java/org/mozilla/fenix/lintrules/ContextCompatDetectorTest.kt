/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.lintrules

import com.android.tools.lint.checks.infrastructure.LintDetectorTest
import com.android.tools.lint.detector.api.Detector
import com.android.tools.lint.detector.api.Issue
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.JUnit4

@RunWith(JUnit4::class)
class ContextCompatDetectorTest : LintDetectorTest() {

    override fun getDetector(): Detector = ContextCompatDetector()

    override fun getIssues(): MutableList<Issue> = ContextCompatDetector.ISSUES.toMutableList()

    @Test
    fun `report error when the ContextCompat getDrawable method is called`() {
        val code = """
            |package example
            |
            |import androidx.core.content.ContextCompat
            |import android.content.Context
            |
            |class Fake {
            |
            |   fun trigger(context: Context, id: Int) {
            |       ContextCompat.getDrawable(context, id)
            |   }
            |
            |}""".trimMargin()

        val expectedReport = """
            |src/example/Fake.kt:9: Error: This call can lead to crashes, replace with AppCompatResources.getDrawable [UnsafeCompatGetDrawable]
            |       ContextCompat.getDrawable(context, id)
            |       ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
            |1 errors, 0 warnings""".trimMargin()

        val expectedFixOutput = """
            |Fix for src/example/Fake.kt line 9: Replace with AppCompatResources.getDrawable:
            |@@ -9 +9
            |-        ContextCompat.getDrawable(context, id)
            |+        AppCompatResources.getDrawable(context, id)""".trimMargin()

        lint()
            .files(Context, ContextCompat, kotlin(code))
            .allowMissingSdk(true)
            .run()
            .expect(expectedReport)
            .expectFixDiffs(expectedFixOutput)
    }

    @Test
    fun `report error when the ContextCompat getColorStateList method is called`() {
        val code = """
            |package example
            |
            |import androidx.core.content.ContextCompat
            |import android.content.Context
            |
            |class Fake {
            |
            |   fun trigger(context: Context, id: Int) {
            |       ContextCompat.getColorStateList(context, id)
            |   }
            |
            |}""".trimMargin()

        val expectedReport = """
            |src/example/Fake.kt:9: Error: This call can lead to crashes, replace with AppCompatResources.getColorStateList [UnsafeCompatGetColorStateList]
            |       ContextCompat.getColorStateList(context, id)
            |       ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
            |1 errors, 0 warnings""".trimMargin()

        val expectedFixOutput = """
            |Fix for src/example/Fake.kt line 9: Replace with AppCompatResources.getColorStateList:
            |@@ -9 +9
            |-        ContextCompat.getColorStateList(context, id)
            |+        AppCompatResources.getColorStateList(context, id)""".trimMargin()

        lint()
            .files(Context, ContextCompat, kotlin(code))
            .allowMissingSdk(true)
            .run()
            .expect(expectedReport)
            .expectFixDiffs(expectedFixOutput)
    }

    @Test
    fun `does not report any errors if the getDrawable method being called is not from the ContextCompat class`() {
        val code = """
            |package example
            |
            |import android.content.Context
            |
            |class Fake {
            |
            |   fun trigger(context: Context, id: Int) {
            |       getDrawable(context, id)
            |   }
            |
            |   fun getDrawable(context: Context, id: Int) {}
            |
            |}""".trimMargin()

        lint()
            .files(Context, kotlin(code))
            .allowMissingSdk(true)
            .run()
            .expectClean()
    }

    @Test
    fun `does not report any errors if the getColorStateList method being called is not from the ContextCompat class`() {
        val code = """
            |package example
            |
            |import android.content.Context
            |
            |class Fake {
            |
            |   fun trigger(context: Context, id: Int) {
            |       getColorStateList(context, id)
            |   }
            |
            |   fun getColorStateList(context: Context, id: Int) {}
            |
            |}""".trimMargin()

        lint()
            .files(Context, kotlin(code))
            .allowMissingSdk(true)
            .run()
            .expectClean()
    }

    // As Lint doesn't have access to the real corresponding classes, we have to provide stubs
    companion object Stubs {

        private val Context = java(
            """
            |package android.content;
            |
            |public class Context {}
            |""".trimMargin()
        )

        private val ContextCompat = java(
            """
            |package androidx.core.content;
            |
            |public class ContextCompat {
            |   public static void getDrawable(Context context, int resId) {}
            |   public static void getColorStateList(Context context, int resId) {}
            |}
            |""".trimMargin()
        )
    }
}
