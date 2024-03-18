/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.lintrules.perf

import com.android.resources.ResourceFolderType
import com.android.tools.lint.checks.infrastructure.TestFiles
import com.android.tools.lint.checks.infrastructure.TestLintTask.lint
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Ignore
import org.junit.Test

// FQCN = fully qualified class name
private const val FQCN_CONSTRAINT_LAYOUT = "androidx.constraintlayout.widget.ConstraintLayout"
private const val FQCN_CUSTOM_VIEW = "org.mozilla.fenix.library.LibrarySiteItemView" // chosen arbitrarily

private val ALL_ISSUES = ConstraintLayoutPerfDetector.ISSUES.toTypedArray()

class ConstraintLayoutPerfDetectorTest {

    private lateinit var detector: ConstraintLayoutPerfDetector

    @Before
    fun setUp() {
        detector = ConstraintLayoutPerfDetector()
    }

    @Test
    fun `WHEN checking file types THEN it only applies layout`() {
        val excludedType = ResourceFolderType.LAYOUT

        val allTypeButLayout = ResourceFolderType.values().filter { it != excludedType }
        allTypeButLayout.forEach { assertFalse(detector.appliesTo(it)) }

        assertTrue(detector.appliesTo(excludedType))
    }

    @Test
    fun `WHEN checking applicable elements THEN it applies to ConstraintLayout` () {
        assertTrue(detector.getApplicableElements()!!.contains(
            "androidx.constraintlayout.widget.ConstraintLayout"
        ))
    }

    @Test
    fun `WHEN checking applicable elements THEN it doesnt apply to some common ones`() {
        val applicableElements = detector.getApplicableElements()!!
        listOf(
            "LinearLayout",
            "fragment"
        ).forEach { assertFalse(it, applicableElements.contains(it)) }
    }

    @Test
    fun `GIVEN a file with one ConstraintLayout WHEN detecting THEN pass`() {
        val layout = getLayoutWithConstraintLayoutRoot("<TextView/>")
        lint()
            .files(getTestXml(layout))
            .issues(*ALL_ISSUES)
            .run()
            .expectClean()
    }

    @Test
    fun `GIVEN a file with two ConstraintLayouts WHEN detecting THEN fail`() {
        // Since we're not the root view, we need to indent extra.
        val innerLayout = """<$FQCN_CONSTRAINT_LAYOUT
            |         android:layout_width="match_parent"
            |         android:layout_height="match_parent">
            |
            |     </$FQCN_CONSTRAINT_LAYOUT>
        """.trimMargin()
        val layout = getLayoutWithConstraintLayoutRoot(innerLayout)
        lint()
            .files(getTestXml(layout))
            .issues(*ALL_ISSUES)
            .run()
            .expectWarningCount(0)
            .expectErrorCount(1)
    }

    @Ignore("the lint test API seems to be broken: project0: Error: Unexpected failure during " +
        "lint analysis (this is a bug in lint or one of the libraries it depends on). Message: The " +
        "prefix \"android\" for attribute \"android:layout_width\" associated with an element type \"TextView\" is not bound.")
    @Test
    fun `GIVEN a file with a ConstraintLayout and a custom view ConstraintLayout layout WHEN detecting THEN warning`() {
        val innerLayout = """<TextView
            |         android:layout_width="match_parent"
            |         android:layout_height="match_parent">
            |
            |     </$FQCN_CUSTOM_VIEW>
        """.trimMargin()
        lint()
            .files(getTestXml(innerLayout))
            .issues(*ALL_ISSUES)
            .run()
            .expectWarningCount(1)
            .expectErrorCount(0)
    }

    private fun getTestXml(code: String) = TestFiles.xml("res/layout/activity_home.xml", code)

    private fun getLayoutWithConstraintLayoutRoot(innerLayout: String = "") = """<!-- This Source Code Form is subject to the terms of the Mozilla Public
        | - License, v. 2.0. If a copy of the MPL was not distributed with this
        | - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->
        | <$FQCN_CONSTRAINT_LAYOUT
        |     xmlns:android="http://schemas.android.com/apk/res/android"
        |     android:layout_width="match_parent"
        |     android:layout_height="match_parent">
        |
        |     $innerLayout
        |
        | </$FQCN_CONSTRAINT_LAYOUT>""".trimMargin()
}
