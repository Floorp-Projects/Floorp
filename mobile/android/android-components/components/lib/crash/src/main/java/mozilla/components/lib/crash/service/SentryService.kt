/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.service

import android.content.Context
import android.net.Uri
import androidx.annotation.VisibleForTesting
import io.sentry.SentryClient
import io.sentry.SentryClientFactory
import io.sentry.android.AndroidSentryClientFactory
import io.sentry.event.Breadcrumb
import io.sentry.event.BreadcrumbBuilder
import mozilla.components.Build
import mozilla.components.lib.crash.Crash

/**
 * A [CrashReporterService] implementation that uploads crash reports to a Sentry server.
 *
 * This implementation will add default tags to every sent crash report (like the used Android Components version)
 * prefixed with "ac.".
 *
 * @param context The application [Context].
 * @param dsn Data Source Name of the Sentry server.
 * @param tags A list of additional tags that will be sent together with crash reports.
 * @param environment An optional, environment name string or null to set none
 */
class SentryService(
    context: Context,
    dsn: String,
    tags: Map<String, String> = emptyMap(),
    environment: String? = null,
    private val sendEventForNativeCrashes: Boolean = false,
    clientFactory: SentryClientFactory = AndroidSentryClientFactory(context)
) : CrashReporterService {
    private val client: SentryClient by lazy { SentryClientFactory.sentryClient(
            Uri.parse(dsn).buildUpon()
                .appendQueryParameter("uncaught.handler.enabled", "false")
                .build()
                .toString(),
            clientFactory).apply {
            this.environment = environment
            tags.forEach { entry ->
                addTag(entry.key, entry.value)
            }

            // Add default tags
            addTag("ac.version", Build.version)
            addTag("ac.git", Build.gitHash)
            addTag("ac.as.build_version", Build.applicationServicesVersion)
        }
    }

    override fun report(crash: Crash.UncaughtExceptionCrash) {
        crash.breadcrumbs.forEach {
            client.context.recordBreadcrumb(it.toSentryBreadcrumb())
        }
        client.sendException(crash.throwable)
    }

    override fun report(crash: Crash.NativeCodeCrash) {
        if (sendEventForNativeCrashes) {
            crash.breadcrumbs.forEach {
                client.context.recordBreadcrumb(it.toSentryBreadcrumb())
            }
            client.sendMessage(createMessage(crash))
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun mozilla.components.lib.crash.Breadcrumb.toSentryBreadcrumb(): Breadcrumb {
        return BreadcrumbBuilder()
                .setMessage(this.message)
                .setData(this.data)
                .setCategory(this.category)
                .setLevel(this.level.toSentryBreadcrumbLevel())
                .setType(this.type.toSentryBreadcrumbType())
                .setTimestamp(this.date)
                .build()
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun mozilla.components.lib.crash.Breadcrumb.Level.toSentryBreadcrumbLevel() = when (this) {
        mozilla.components.lib.crash.Breadcrumb.Level.CRITICAL -> Breadcrumb.Level.CRITICAL
        mozilla.components.lib.crash.Breadcrumb.Level.ERROR -> Breadcrumb.Level.ERROR
        mozilla.components.lib.crash.Breadcrumb.Level.WARNING -> Breadcrumb.Level.WARNING
        mozilla.components.lib.crash.Breadcrumb.Level.INFO -> Breadcrumb.Level.INFO
        mozilla.components.lib.crash.Breadcrumb.Level.DEBUG -> Breadcrumb.Level.DEBUG
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun mozilla.components.lib.crash.Breadcrumb.Type.toSentryBreadcrumbType() = when (this) {
        mozilla.components.lib.crash.Breadcrumb.Type.DEFAULT -> Breadcrumb.Type.DEFAULT
        mozilla.components.lib.crash.Breadcrumb.Type.HTTP -> Breadcrumb.Type.HTTP
        mozilla.components.lib.crash.Breadcrumb.Type.NAVIGATION -> Breadcrumb.Type.NAVIGATION
        mozilla.components.lib.crash.Breadcrumb.Type.USER -> Breadcrumb.Type.USER
    }
}

private fun createMessage(crash: Crash.NativeCodeCrash): String {
    val fatal = crash.isFatal.toString()
    val minidumpSuccess = crash.minidumpSuccess

    return "NativeCodeCrash(fatal=$fatal, minidumpSuccess=$minidumpSuccess)"
}
