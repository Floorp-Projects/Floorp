/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.sentry.legacy

import android.content.Context
import android.net.Uri
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.Companion.PRIVATE
import io.sentry.SentryClient
import io.sentry.SentryClientFactory
import io.sentry.android.AndroidSentryClientFactory
import io.sentry.event.Breadcrumb
import io.sentry.event.BreadcrumbBuilder
import io.sentry.event.Event
import io.sentry.event.EventBuilder
import io.sentry.event.interfaces.ExceptionInterface
import mozilla.components.Build
import mozilla.components.lib.crash.Crash
import mozilla.components.lib.crash.service.CrashReporterService
import mozilla.components.lib.crash.service.LIB_CRASH_INFO_PREFIX
import java.util.Date
import java.util.Locale
import mozilla.components.concept.base.crash.Breadcrumb as CrashBreadcrumb

/**
 * A [CrashReporterService] implementation that uploads crash reports using
 * the Sentry SDK version 1.7.21 and below.
 *
 * This implementation will add default tags to every sent crash report
 * (like which Android Components version is being used) prefixed with "ac.".
 *
 * @param context The application [Context].
 * @param dsn Data Source Name of the Sentry server.
 * @param tags A list of additional tags that will be sent together with crash reports.
 * @param environment An optional, environment name string or null to set none
 * @param sentryProjectUrl Base URL of the Sentry web interface pointing to the app/project.
 */
class SentryService(
    context: Context,
    dsn: String,
    tags: Map<String, String> = emptyMap(),
    environment: String? = null,
    private val sendEventForNativeCrashes: Boolean = false,
    private val sentryProjectUrl: String? = null,
    clientFactory: SentryClientFactory? = null,
) : CrashReporterService {
    override val id: String = "sentry"

    override val name: String = "Sentry"

    override fun createCrashReportUrl(identifier: String): String? {
        return sentryProjectUrl?.let {
            val id = identifier.replace("-", "")
            return "$it/?query=$id"
        }
    }

    // Fenix perf note: Sentry init may negatively impact cold startup so it's important this is lazily init.
    @VisibleForTesting(otherwise = PRIVATE)
    internal val client: SentryClient by lazy {
        SentryClientFactory.sentryClient(
            Uri.parse(dsn).buildUpon()
                .appendQueryParameter("uncaught.handler.enabled", "false")
                .build()
                .toString(),

            // Fenix perf note: we initialize Android...Factory inside the lazy block to avoid
            // calling the slow Logger.getLogger call on cold startup #7441
            clientFactory ?: AndroidSentryClientFactory(context),
        ).apply {
            this.environment = environment
            tags.forEach { entry ->
                addTag(entry.key, entry.value)
            }

            // Add default tags
            addTag("ac.version", Build.version)
            addTag("ac.git", Build.gitHash)
            addTag("ac.as.build_version", Build.applicationServicesVersion)
            addTag("ac.glean.build_version", Build.gleanSdkVersion)
            addTag("user.locale", Locale.getDefault().toString())
        }
    }

    override fun report(crash: Crash.UncaughtExceptionCrash): String? {
        crash.breadcrumbs.forEach {
            client.context.recordBreadcrumb(it.toSentryBreadcrumb())
        }

        val eventBuilder = EventBuilder().withMessage(createMessage(crash))
            .withTimestamp(Date(crash.timestamp))
            .withLevel(Event.Level.FATAL)
            .withSentryInterface(ExceptionInterface(crash.throwable))

        client.sendEvent(eventBuilder)
        client.context.clearBreadcrumbs()

        return eventBuilder.event.id.toString()
    }

    override fun report(crash: Crash.NativeCodeCrash): String? {
        if (sendEventForNativeCrashes) {
            val level = when (crash.isFatal) {
                true -> Event.Level.FATAL
                else -> Event.Level.ERROR
            }

            crash.breadcrumbs.forEach {
                client.context.recordBreadcrumb(it.toSentryBreadcrumb())
            }

            val eventBuilder = EventBuilder()
                .withTimestamp(Date(crash.timestamp))
                .withMessage(createMessage(crash))
                .withLevel(level)

            client.sendEvent(eventBuilder)
            client.context.clearBreadcrumbs()

            return eventBuilder.event.id.toString()
        }

        return null
    }

    override fun report(throwable: Throwable, breadcrumbs: ArrayList<CrashBreadcrumb>): String? {
        breadcrumbs.forEach {
            client.context.recordBreadcrumb(it.toSentryBreadcrumb())
        }

        val eventBuilder = EventBuilder().withMessage(createMessage(throwable))
            .withLevel(Event.Level.INFO)
            .withSentryInterface(ExceptionInterface(throwable))

        client.sendEvent(eventBuilder)
        client.context.clearBreadcrumbs()

        return eventBuilder.event.id.toString()
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun CrashBreadcrumb.toSentryBreadcrumb(): Breadcrumb {
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
    internal fun CrashBreadcrumb.Level.toSentryBreadcrumbLevel() = when (this) {
        CrashBreadcrumb.Level.CRITICAL -> Breadcrumb.Level.CRITICAL
        CrashBreadcrumb.Level.ERROR -> Breadcrumb.Level.ERROR
        CrashBreadcrumb.Level.WARNING -> Breadcrumb.Level.WARNING
        CrashBreadcrumb.Level.INFO -> Breadcrumb.Level.INFO
        CrashBreadcrumb.Level.DEBUG -> Breadcrumb.Level.DEBUG
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun CrashBreadcrumb.Type.toSentryBreadcrumbType() = when (this) {
        CrashBreadcrumb.Type.DEFAULT -> Breadcrumb.Type.DEFAULT
        CrashBreadcrumb.Type.HTTP -> Breadcrumb.Type.HTTP
        CrashBreadcrumb.Type.NAVIGATION -> Breadcrumb.Type.NAVIGATION
        CrashBreadcrumb.Type.USER -> Breadcrumb.Type.USER
    }
}

private fun createMessage(crash: Crash.NativeCodeCrash): String {
    val fatal = crash.isFatal.toString()
    val processType = crash.processType
    val minidumpSuccess = crash.minidumpSuccess

    return "NativeCodeCrash(fatal=$fatal, processType=$processType, minidumpSuccess=$minidumpSuccess)"
}

private fun createMessage(crash: Crash.UncaughtExceptionCrash): String {
    return crash.throwable.message ?: ""
}

private fun createMessage(throwable: Throwable): String {
    return "$LIB_CRASH_INFO_PREFIX ${throwable.message}"
}
