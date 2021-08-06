/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("UnstableApiUsage")

package org.mozilla.fenix.lintrules

import com.android.tools.lint.client.api.UElementHandler
import com.android.tools.lint.detector.api.Category
import com.android.tools.lint.detector.api.Detector
import com.android.tools.lint.detector.api.Implementation
import com.android.tools.lint.detector.api.Issue
import com.android.tools.lint.detector.api.JavaContext
import com.android.tools.lint.detector.api.Scope
import com.android.tools.lint.detector.api.Severity
import com.android.tools.lint.detector.api.SourceCodeScanner
import org.jetbrains.uast.UElement
import org.jetbrains.uast.UImportStatement

/**
 * Custom Lint detector that checks for deprecated "kotlinx.android.synthetic" imports.
 */
class KotlinSyntheticsDetector : Detector(), SourceCodeScanner {

    override fun createUastHandler(context: JavaContext) = object : UElementHandler() {
        override fun visitImportStatement(node: UImportStatement) {
            node.importReference?.asSourceString()?.let { import ->
                if (import.startsWith(SYNTHETIC_IMPORT_PATTERN)) {
                    context.report(
                        DEPRECATED_KOTLIN_SYNTHETICS_ISSUE,
                        node,
                        context.getLocation(node),
                        DEPRECATED_KOTLIN_SYNTHETICS_MESSAGE
                    )
                }
            }
        }
    }

    override fun getApplicableUastTypes(): List<Class<out UElement>> {
        return listOf(UImportStatement::class.java)
    }

    companion object {
        private const val DEPRECATED_KOTLIN_SYNTHETICS_MESSAGE = "Kotlin synthetics are to be replaced with Jetpack View Binding. " +
            "Please refer to https://github.com/mozilla-mobile/fenix/issues/17917 for more details."
        private const val SYNTHETIC_IMPORT_PATTERN = "kotlinx.android.synthetic"

        val DEPRECATED_KOTLIN_SYNTHETICS_ISSUE = Issue.create(
            "KotlinSyntheticsDeprecation",
            briefDescription = "Kotlin synthetics are deprecated and actively refactored out",
            explanation = "The 'kotlin-android-extensions' Gradle plugin is deprecated and " +
                "will be removed in a future Kotlin release in (or after) September 2021. " +
                "We are to migrate to Jetpack View Binding. " +
                "Please refer to https://github.com/mozilla-mobile/fenix/issues/17917",
            category = Category.PRODUCTIVITY,
            severity = Severity.ERROR,
            implementation = Implementation(KotlinSyntheticsDetector::class.java, Scope.JAVA_FILE_SCOPE)
        )
    }
}
