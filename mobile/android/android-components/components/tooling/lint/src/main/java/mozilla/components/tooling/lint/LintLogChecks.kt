/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.tooling.lint

import com.android.tools.lint.detector.api.Category
import com.android.tools.lint.detector.api.Detector
import com.android.tools.lint.detector.api.Implementation
import com.android.tools.lint.detector.api.Issue
import com.android.tools.lint.detector.api.JavaContext
import com.android.tools.lint.detector.api.Scope
import com.android.tools.lint.detector.api.Severity
import com.intellij.psi.PsiMethod
import org.jetbrains.uast.UCallExpression
import org.jetbrains.uast.getContainingUClass
import java.util.EnumSet

internal const val ANDROID_LOG_CLASS = "android.util.Log"
internal const val ERROR_MESSAGE = "Using Android Log instead of base component"

/**
 * Custom lint checks related to logging.
 */
class LintLogChecks : Detector(), Detector.UastScanner {
    private val componentPackages = listOf("mozilla.components", "org.mozilla.telemetry", "org.mozilla.samples")

    override fun getApplicableMethodNames() = listOf("v", "d", "i", "w", "e")

    override fun visitMethodCall(context: JavaContext, node: UCallExpression, method: PsiMethod) {
        if (context.evaluator.isMemberInClass(method, ANDROID_LOG_CLASS)) {
            val inComponentPackage = componentPackages.any {
                node.methodIdentifier?.getContainingUClass()?.qualifiedName?.startsWith(it) == true
            }

            if (inComponentPackage) {
                context.report(
                    ISSUE_LOG_USAGE,
                    node,
                    context.getLocation(node),
                    ERROR_MESSAGE,
                )
            }
        }
    }

    companion object {
        internal val ISSUE_LOG_USAGE = Issue.create(
            "LogUsage",
            "Log/Logger from base component should be used.",
            """The Log or Logger class from the base component should be used for logging instead of
            Android's Log class. This will allow the app to control what logs should be accepted
            and how they should be processed.
            """.trimIndent(),
            Category.MESSAGES,
            5,
            Severity.WARNING,
            Implementation(LintLogChecks::class.java, EnumSet.of(Scope.JAVA_FILE)),
        )
    }
}
