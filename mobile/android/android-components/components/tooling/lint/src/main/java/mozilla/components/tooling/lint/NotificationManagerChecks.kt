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

internal const val ANDROID_NOTIFICATION_MANAGER_COMPAT_CLASS =
    "androidx.core.app.NotificationManagerCompat"
internal const val ANDROID_NOTIFICATION_MANAGER_CLASS =
    "android.app.NotificationManager"

internal const val NOTIFY_ERROR_MESSAGE = "Using Android NOTIFY instead of base component"

/**
 * Custom lint that ensures [NotificationManagerCompat] and [NotificationManager]'s method [notify]
 * is not called directly from code.
 * Calling notify directly from code eludes the checks implemented in [NotificationsDelegate]
 */
class NotificationManagerChecks : Detector(), Detector.UastScanner {
    private val componentPackages =
        listOf("mozilla.components", "org.mozilla.telemetry", "org.mozilla.samples")
    private val appPackages = listOf("org.mozilla.fenix", "org.mozilla.focus")

    override fun getApplicableMethodNames() = listOf("notify")

    override fun visitMethodCall(context: JavaContext, node: UCallExpression, method: PsiMethod) {
        if (context.evaluator.isMemberInClass(method, ANDROID_NOTIFICATION_MANAGER_COMPAT_CLASS) ||
            context.evaluator.isMemberInClass(method, ANDROID_NOTIFICATION_MANAGER_CLASS)
        ) {
            val inComponentPackage = componentPackages.any {
                node.methodIdentifier?.getContainingUClass()?.qualifiedName?.startsWith(it) == true
            }

            val inAppPackage = appPackages.any {
                node.methodIdentifier?.getContainingUClass()?.qualifiedName?.startsWith(it) == true
            }

            if (inComponentPackage) {
                context.report(
                    ISSUE_NOTIFICATION_USAGE,
                    node,
                    context.getLocation(node),
                    NOTIFY_ERROR_MESSAGE,
                )
            }

            if (inAppPackage) {
                context.report(
                    ISSUE_NOTIFICATION_USAGE,
                    node,
                    context.getLocation(node),
                    NOTIFY_ERROR_MESSAGE,
                )
            }
        }
    }

    companion object {
        internal val ISSUE_NOTIFICATION_USAGE = Issue.create(
            "NotifyUsage",
            "NotificationsDelegate should be used instead of NotificationManager.",
            """NotificationsDelegate should be used for showing notifications instead of a NotificationManager
            or a NotificationManagerCompat. This will allow the app to control requesting the notification permission 
            when needed and handling the request result.
            """.trimIndent(),
            Category.MESSAGES,
            5,
            Severity.WARNING,
            Implementation(NotificationManagerChecks::class.java, EnumSet.of(Scope.JAVA_FILE)),
        )
    }
}
