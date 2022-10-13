/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.tooling.lint

import com.android.tools.lint.client.api.JavaEvaluator
import com.android.tools.lint.detector.api.JavaContext
import com.intellij.psi.PsiMethod
import mozilla.components.tooling.lint.LintLogChecks.Companion.ISSUE_LOG_USAGE
import org.jetbrains.uast.UCallExpression
import org.jetbrains.uast.UClass
import org.jetbrains.uast.UIdentifier
import org.jetbrains.uast.getContainingUClass
import org.junit.Ignore
import org.junit.Test
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`

class LintLogChecksTest {

    @Test
    @Ignore("With Java 11 Mockito fails to mock some of the Android classes here")
    fun `report log error in components code only`() {
        val evaluator = mock(JavaEvaluator::class.java)
        val context = mock(JavaContext::class.java)
        val node = mock(UCallExpression::class.java)
        val method = mock(PsiMethod::class.java)
        val methodIdentifier = mock(UIdentifier::class.java)
        val clazz = mock(UClass::class.java)

        `when`(evaluator.isMemberInClass(method, ANDROID_LOG_CLASS)).thenReturn(true)
        `when`(context.evaluator).thenReturn(evaluator)

        val logCheck = LintLogChecks()
        logCheck.visitMethodCall(context, node, method)
        verify(context, never()).report(ISSUE_LOG_USAGE, node, context.getLocation(node), ERROR_MESSAGE)

        `when`(node.methodIdentifier).thenReturn(methodIdentifier)
        logCheck.visitMethodCall(context, node, method)
        verify(context, never()).report(ISSUE_LOG_USAGE, node, context.getLocation(node), ERROR_MESSAGE)

        `when`(methodIdentifier.getContainingUClass()).thenReturn(clazz)
        logCheck.visitMethodCall(context, node, method)
        verify(context, never()).report(ISSUE_LOG_USAGE, node, context.getLocation(node), ERROR_MESSAGE)

        `when`(clazz.qualifiedName).thenReturn("com.some.app.Class")
        logCheck.visitMethodCall(context, node, method)
        verify(context, never()).report(ISSUE_LOG_USAGE, node, context.getLocation(node), ERROR_MESSAGE)

        `when`(clazz.qualifiedName).thenReturn("mozilla.components.some.Class")
        logCheck.visitMethodCall(context, node, method)
        verify(context, times(1)).report(ISSUE_LOG_USAGE, node, context.getLocation(node), ERROR_MESSAGE)
    }
}
