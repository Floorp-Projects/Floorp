/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.tooling.lint

import com.android.tools.lint.checks.DataFlowAnalyzer
import com.android.tools.lint.detector.api.Category
import com.android.tools.lint.detector.api.Detector
import com.android.tools.lint.detector.api.Implementation
import com.android.tools.lint.detector.api.Issue
import com.android.tools.lint.detector.api.JavaContext
import com.android.tools.lint.detector.api.Scope
import com.android.tools.lint.detector.api.Severity
import com.android.tools.lint.detector.api.SourceCodeScanner
import com.intellij.psi.PsiMethod
import org.jetbrains.uast.UCallExpression
import org.jetbrains.uast.UElement
import org.jetbrains.uast.UMethod
import org.jetbrains.uast.UReturnExpression
import org.jetbrains.uast.getParentOfType

/**
 * A custom lint check that warns if [Fact.collect()] is not called on a newly created [Fact] instance
 */
class FactCollectDetector : Detector(), SourceCodeScanner {

    companion object {
        private const val FULLY_QUALIFIED_FACT_CLASS_NAME =
            "mozilla.components.support.base.facts.Fact"
        private const val EXPECTED_METHOD_SIMPLE_NAME =
            "collect" // The `Fact.collect` extension method

        private val IMPLEMENTATION = Implementation(
            FactCollectDetector::class.java,
            Scope.JAVA_FILE_SCOPE,
        )

        val ISSUE_FACT_COLLECT_CALLED: Issue = Issue
            .create(
                id = "FactCollect",
                briefDescription = "Fact created but not collected",
                explanation = """
                An instance of `Fact` was created but not collected. You must call 
                `collect()` on the instance to actually process it.
                """.trimIndent(),
                category = Category.CORRECTNESS,
                priority = 6,
                severity = Severity.ERROR,
                implementation = IMPLEMENTATION,
            )
    }

    override fun getApplicableConstructorTypes(): List<String> {
        return listOf(FULLY_QUALIFIED_FACT_CLASS_NAME)
    }

    override fun visitConstructor(
        context: JavaContext,
        node: UCallExpression,
        constructor: PsiMethod,
    ) {
        var isCollectCalled = false
        var escapes = false
        val visitor = object : DataFlowAnalyzer(setOf(node)) {
            override fun receiver(call: UCallExpression) {
                if (call.methodName == EXPECTED_METHOD_SIMPLE_NAME) {
                    isCollectCalled = true
                }
            }

            override fun argument(call: UCallExpression, reference: UElement) {
                escapes = true
            }

            override fun field(field: UElement) {
                escapes = true
            }

            override fun returns(expression: UReturnExpression) {
                escapes = true
            }
        }
        val method = node.getParentOfType<UMethod>(UMethod::class.java, true) ?: return
        method.accept(visitor)
        if (!isCollectCalled && !escapes) {
            reportUsage(context, node)
        }
    }

    private fun reportUsage(context: JavaContext, node: UCallExpression) {
        context.report(
            issue = ISSUE_FACT_COLLECT_CALLED,
            scope = node,
            location = context.getCallLocation(
                call = node,
                includeReceiver = true,
                includeArguments = false,
            ),
            message = "Fact created but not shown: did you forget to call `collect()` ?",
        )
    }
}
