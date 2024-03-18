/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.tooling.lint

import com.android.tools.lint.checks.infrastructure.LintDetectorTest
import com.android.tools.lint.detector.api.Detector
import com.android.tools.lint.detector.api.Issue
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.JUnit4

/**
 * Tests for the [AndroidSrcXmlDetector] custom lint check.
 */
@RunWith(JUnit4::class)
class AndroidSrcXmlDetectorTest : LintDetectorTest() {

    override fun getIssues(): MutableList<Issue> =
        mutableListOf(AndroidSrcXmlDetector.ISSUE_XML_SRC_USAGE)

    override fun getDetector(): Detector = AndroidSrcXmlDetector()

    @Test
    fun expectPass() {
        lint()
            .files(
                xml(
                    "res/layout/layout.xml",
                    """
<ImageView xmlns:android="http://schemas.android.com/apk/res/android"
    android:layout_width="wrap_content"
    android:layout_height="wrap_content"
    />
""",
                ),
            ).allowMissingSdk(true)
            .run()
            .expectClean()
    }

    @Test
    fun expectFail() {
        lint()
            .files(
                xml(
                    "res/layout/layout.xml",
                    """
<ImageView xmlns:android="http://schemas.android.com/apk/res/android"
    android:layout_width="wrap_content"
    android:layout_height="wrap_content"
    android:src="@drawable/ic_close"
    />
""",
                ),
            ).allowMissingSdk(true)
            .run()
            .expect(
                """
res/layout/layout.xml:5: Error: Using android:src to define resource instead of app:srcCompat [AndroidSrcXmlDetector]
    android:src="@drawable/ic_close"
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
1 errors, 0 warnings
            """,
            )
    }
}
