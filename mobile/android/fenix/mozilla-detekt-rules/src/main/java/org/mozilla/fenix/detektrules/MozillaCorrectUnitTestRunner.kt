/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.detektrules

import io.gitlab.arturbosch.detekt.api.CodeSmell
import io.gitlab.arturbosch.detekt.api.Config
import io.gitlab.arturbosch.detekt.api.Debt
import io.gitlab.arturbosch.detekt.api.Entity
import io.gitlab.arturbosch.detekt.api.Issue
import io.gitlab.arturbosch.detekt.api.Rule
import io.gitlab.arturbosch.detekt.api.Severity
import io.gitlab.arturbosch.detekt.api.internal.PathFilters
import org.jetbrains.kotlin.psi.KtAnnotationEntry

private val BANNED_TEST_RUNNERS = setOf(
    // When updating this list, also update the violation message.
    "AndroidJUnit4",
    "RobolectricTestRunner",
)

// There is a change to how we output violations in a different PR so the formatting for message
// might be weird but it should still be readable.
private val VIOLATION_MSG = """The following code blocks:
    |
    |        @RunWith(AndroidJUnit4::class)
    |        @Config(application = TestApplication::class)
    |    OR
    |        @RunWith(RobolectricTestRunner::class)
    |        @Config(application = TestApplication::class)
    |
    |    should be replaced with:
    |
    |        @RunWith(FenixRobolectricTestRunner::class)
    |
    |    To reduce redundancy of setting @Config. No @Config specification is necessary because
    |    the FenixRobolectricTestRunner sets it automatically.
    |
    |    Relatedly, adding robolectric to a test increases its runtime non-trivially so please
    |    ensure Robolectric is necessary before adding it.""".trimMargin()

/**
 * Ensures we're using the correct Robolectric unit test runner.
 */
class MozillaCorrectUnitTestRunner(config: Config) : Rule(config) {
    override val issue = Issue(
        MozillaCorrectUnitTestRunner::class.simpleName!!,
        Severity.Defect,
        "Verifies we're using the correct Robolectric unit test runner",
        Debt.FIVE_MINS,
    )

    override val filters: PathFilters?
        get() = PathFilters.of(
            includes = listOf("**/test/**"), // unit tests only.
            excludes = emptyList(),
        )

    override fun visitAnnotationEntry(annotationEntry: KtAnnotationEntry) {
        super.visitAnnotationEntry(annotationEntry)

        // Example of an annotation: @RunWith(FenixRobolectricUnitTestRunner::class)
        val annotationClassName = annotationEntry.shortName?.identifier
        if (annotationClassName == "RunWith") {
            // Example arg in parens: "(FenixRobolectricUnitTestRunner::class)"
            val argInParens = annotationEntry.lastChild.node.chars
            val argClassName = argInParens.substring(1 until argInParens.indexOf(":"))

            val isInViolation = BANNED_TEST_RUNNERS.any { it == argClassName }
            if (isInViolation) {
                report(CodeSmell(issue, Entity.from(annotationEntry), VIOLATION_MSG))
            }
        }
    }
}
