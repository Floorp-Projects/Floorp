/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.tooling.lint

import com.android.tools.lint.checks.CheckResultDetector
import com.android.tools.lint.checks.DataFlowAnalyzer
import com.android.tools.lint.checks.EscapeCheckingDataFlowAnalyzer
import com.android.tools.lint.checks.TargetMethodDataFlowAnalyzer
import com.android.tools.lint.checks.isMissingTarget
import com.android.tools.lint.detector.api.Category
import com.android.tools.lint.detector.api.Detector
import com.android.tools.lint.detector.api.Implementation
import com.android.tools.lint.detector.api.Issue
import com.android.tools.lint.detector.api.JavaContext
import com.android.tools.lint.detector.api.Scope
import com.android.tools.lint.detector.api.Severity
import com.android.tools.lint.detector.api.SourceCodeScanner
import com.android.tools.lint.detector.api.getUMethod
import com.android.tools.lint.detector.api.isJava
import com.android.tools.lint.detector.api.skipLabeledExpression
import com.intellij.psi.LambdaUtil
import com.intellij.psi.PsiElement
import com.intellij.psi.PsiMethod
import com.intellij.psi.PsiResourceVariable
import com.intellij.psi.PsiVariable
import com.intellij.psi.util.PsiTreeUtil
import org.jetbrains.uast.UCallExpression
import org.jetbrains.uast.UCallableReferenceExpression
import org.jetbrains.uast.UElement
import org.jetbrains.uast.UMethod
import org.jetbrains.uast.UQualifiedReferenceExpression
import org.jetbrains.uast.getParentOfType

/**
 * Checks for missing [mozilla.components.concept.fetch.Response.close] call on fetches that might not have used the
 * resources.
 *
 * Review the unit tests for examples on what this [Detector] can identify.
 */
class ConceptFetchDetector : Detector(), SourceCodeScanner {
    override fun getApplicableMethodNames(): List<String> {
        return listOf("fetch")
    }

    override fun visitMethodCall(
        context: JavaContext,
        node: UCallExpression,
        method: PsiMethod,
    ) {
        val containingClass = method.containingClass ?: return
        val evaluator = context.evaluator

        if (evaluator.extendsClass(
                containingClass,
                CLIENT_CLS,
                false,
            )
        ) {
            val returnType = method.getUMethod()?.returnTypeReference ?: return
            val qualifiedName = returnType.getQualifiedName()
            if (qualifiedName != null && qualifiedName == RESPONSE_CLS) {
                checkClosed(context, node)
            }
        }
    }

    @Suppress("ReturnCount") // Extracted from `CleanupDetector#checkRecycled`.
    private fun checkClosed(context: JavaContext, node: UCallExpression) {
        // If it's an AutoCloseable in a try-with-resources clause, don't flag it: these will be
        // cleaned up automatically
        if (node.sourcePsi.isTryWithResources()) {
            return
        }

        val parentMethod = node.getParentOfType(UMethod::class.java) ?: return

        // Check if any of the 'body' methods are used. They are all closeable; do not report.
        val bodyMethodTracker = BodyMethodTracker(listOf(node))
        if (parentMethod.wasMethodCalled(bodyMethodTracker)) {
            return
        }

        // Check if response has escaped (particularly through an extension function); do not report.
        val responseEscapedTracker = ResponseEscapedTracker(listOf(node))
        if (parentMethod.hasEscaped(responseEscapedTracker)) {
            return
        }

        // Check if 'use' or 'close' were called; do not report.
        val closeableTracker = CloseableTracker(listOf(node), context)
        if (!parentMethod.isMissingTarget(closeableTracker)) {
            return
        }

        context.report(
            ISSUE_FETCH_RESPONSE_CLOSE,
            node,
            context.getCallLocation(node, includeReceiver = true, includeArguments = false),
            "Response created but not closed: did you forget to call `close()`?",
            if (CheckResultDetector.isExpressionValueUnused(node)) {
                fix()
                    .replace()
                    .name("Call close()")
                    .range(context.getLocation(node))
                    .end()
                    .with(".close()")
                    .build()
            } else {
                null
            },
        )
    }

    private fun UMethod.hasEscaped(analyzer: EscapeCheckingDataFlowAnalyzer): Boolean {
        accept(analyzer)
        return analyzer.escaped
    }

    private fun UMethod.wasMethodCalled(analyzer: BodyMethodTracker): Boolean {
        accept(analyzer)
        return analyzer.found
    }

    private fun PsiElement?.isTryWithResources(): Boolean {
        return this != null &&
            isJava(this) &&
            PsiTreeUtil.getParentOfType(this, PsiResourceVariable::class.java) != null
    }

    private class BodyMethodTracker(
        initial: Collection<UElement>,
        initialReferences: Collection<PsiVariable> = emptyList(),
    ) : DataFlowAnalyzer(initial, initialReferences) {
        var found = false
        override fun visitQualifiedReferenceExpression(node: UQualifiedReferenceExpression): Boolean {
            val methodName: String? = with(node.selector as? UCallExpression) {
                this?.methodName ?: this?.methodIdentifier?.name
            }

            when (methodName) {
                USE_STREAM,
                USE_BUFFERED_READER,
                STRING,
                -> {
                    if (node.receiver.getExpressionType()?.canonicalText == BODY_CLS) {
                        // We are using any of the `body` methods which are all closeable.
                        found = true
                        return true
                    }
                }
            }

            return super.visitQualifiedReferenceExpression(node)
        }
    }

    private class ResponseEscapedTracker(
        initial: Collection<UElement>,
    ) : EscapeCheckingDataFlowAnalyzer(initial, emptyList()) {
        override fun returnsSelf(call: UCallExpression): Boolean {
            val type = call.receiver?.getExpressionType()?.canonicalText ?: return super.returnsSelf(call)
            return type == RESPONSE_CLS
        }
    }

    // Extracted from `CleanupDetector#checkRecycled#visitor`.
    private class CloseableTracker(
        initial: Collection<UElement>,
        private val context: JavaContext,
    ) : TargetMethodDataFlowAnalyzer(initial, emptyList()) {
        override fun isTargetMethodName(name: String): Boolean {
            return name == USE || name == CLOSE
        }

        @Suppress("ReturnCount")
        override fun isTargetMethod(
            name: String,
            method: PsiMethod?,
            call: UCallExpression?,
            methodRef: UCallableReferenceExpression?,
        ): Boolean {
            if (USE == name) {
                // Kotlin: "use" calls close;
                // Ensure that "use" call accepts a single lambda parameter, so that it would
                // loosely match kotlin.io.use() signature and at the same time allow custom
                // overloads for types not extending Closeable
                if (call != null && call.valueArgumentCount == 1) {
                    val argumentType =
                        call.valueArguments.first().skipLabeledExpression().getExpressionType()
                    if (argumentType != null && LambdaUtil.isFunctionalType(argumentType)) {
                        return true
                    }
                }
                return false
            }

            if (method != null) {
                val containingClass = method.containingClass
                val targetName = containingClass?.qualifiedName ?: return true
                if (targetName == RESPONSE_CLS) {
                    return true
                }
                val recycleClass =
                    context.evaluator.findClass(RESPONSE_CLS) ?: return true
                return context.evaluator.extendsClass(recycleClass, targetName, false)
            } else {
                // Unresolved method call -- assume it's okay
                return true
            }
        }
    }

    companion object {
        @JvmField
        val ISSUE_FETCH_RESPONSE_CLOSE = Issue.create(
            id = "FetchResponseClose",
            briefDescription = "Response stream fetched but not closed.",
            explanation = """
                A `Client.fetch` returns a `Response` that, on success, is consumed typically with 
                a `use` stream in Kotlin or a try-with-resources in Java. In the failure or manual 
                resource managed cases, we need to ensure that `Response.close` is always called.
                
                Additionally, all methods on `Response.body` are AutoCloseable so using any of 
                those will release those resources after execution.
            """.trimIndent(),
            category = Category.CORRECTNESS,
            priority = 6,
            severity = Severity.ERROR,
            androidSpecific = true,
            implementation = Implementation(
                ConceptFetchDetector::class.java,
                Scope.JAVA_FILE_SCOPE,
            ),
        )

        // Target method names
        private const val CLOSE = "close"
        private const val USE = "use"
        private const val USE_STREAM = "useStream"
        private const val USE_BUFFERED_READER = "useBufferedReader"
        private const val STRING = "string"

        private const val CLIENT_CLS = "mozilla.components.concept.fetch.Client"
        private const val RESPONSE_CLS = "mozilla.components.concept.fetch.Response"
        private const val BODY_CLS = "mozilla.components.concept.fetch.Response.Body"
    }
}
