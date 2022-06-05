/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.sentry

import android.content.Context
import androidx.annotation.GuardedBy
import androidx.annotation.VisibleForTesting
import io.sentry.Breadcrumb
import io.sentry.Sentry
import io.sentry.SentryLevel
import io.sentry.android.core.SentryAndroid
import io.sentry.protocol.SentryId
import mozilla.components.Build
import mozilla.components.lib.crash.Crash
import mozilla.components.lib.crash.service.CrashReporterService
import java.util.Locale
import mozilla.components.concept.base.crash.Breadcrumb as MozillaBreadcrumb

/**
 * A [CrashReporterService] implementation that uploads crash reports using
 * the Sentry SDK version 5.6.1 and above.
 *
 * This implementation will add default tags to every sent crash report
 * (like which Android Components version is being used) prefixed with "ac".
 *
 * @param applicationContext The application [Context].
 * @param dsn Data Source Name of the Sentry server.
 * @param tags A list of additional tags that will be sent together with crash reports.
 * @param environment An optional, environment name string or null to set none
 * @param sendEventForNativeCrashes Allows configuring if native crashes should be submitted. Disabled by default.
 * @param sentryProjectUrl Base URL of the Sentry web interface pointing to the app/project.
 * @param sendCaughtExceptions Allows configuring if caught exceptions should be submitted. Enabled by default.
 */
class SentryService(
    private val applicationContext: Context,
    private val dsn: String,
    private val tags: Map<String, String> = emptyMap(),
    private val environment: String? = null,
    private val sendEventForNativeCrashes: Boolean = false,
    private val sentryProjectUrl: String? = null,
    private val sendCaughtExceptions: Boolean = true,
) : CrashReporterService {

    override val id: String = "new-sentry-instance"
    override val name: String = "New Sentry Instance"

    @VisibleForTesting
    @GuardedBy("this")
    internal var isInitialized: Boolean = false

    override fun createCrashReportUrl(identifier: String): String? {
        return sentryProjectUrl?.let {
            val id = identifier.replace("-", "")
            return "$it&query=$id"
        }
    }

    override fun report(crash: Crash.UncaughtExceptionCrash): String {
        prepareReport(crash.breadcrumbs, SentryLevel.FATAL)
        return reportToSentry(crash.throwable)
    }

    override fun report(crash: Crash.NativeCodeCrash): String? {
        return if (sendEventForNativeCrashes) {
            val level = when (crash.isFatal) {
                true -> SentryLevel.FATAL
                else -> SentryLevel.ERROR
            }

            prepareReport(crash.breadcrumbs, level)

            return reportToSentry(crash)
        } else {
            null
        }
    }

    override fun report(throwable: Throwable, breadcrumbs: ArrayList<MozillaBreadcrumb>): String? {
        if (!sendCaughtExceptions) {
            return null
        }
        prepareReport(breadcrumbs, SentryLevel.INFO)
        return reportToSentry(throwable)
    }

    @VisibleForTesting
    internal fun reportToSentry(throwable: Throwable): String {
        return Sentry.captureException(throwable).alsoClearBreadcrumbs()
    }

    @VisibleForTesting
    internal fun reportToSentry(crash: Crash.NativeCodeCrash): String {
        return Sentry.captureMessage(createMessage(crash)).alsoClearBreadcrumbs()
    }

    private fun addDefaultTags() {
        Sentry.setTag("ac.version", Build.version)
        Sentry.setTag("ac.git", Build.gitHash)
        Sentry.setTag("ac.as.build_version", Build.applicationServicesVersion)
        Sentry.setTag("ac.glean.build_version", Build.gleanSdkVersion)
        Sentry.setTag("user.locale", Locale.getDefault().toString())
        tags.forEach { entry ->
            Sentry.setTag(entry.key, entry.value)
        }
    }

    @Synchronized
    @VisibleForTesting
    internal fun initIfNeeded() {
        if (isInitialized) {
            return
        }
        initSentry()
        addDefaultTags()
        isInitialized = true
    }

    @VisibleForTesting
    internal fun initSentry() {
        SentryAndroid.init(applicationContext) { options ->
            // Disable uncaught non-native exceptions from being reported.
            // We already have our own uncaught exception handler [ExceptionHandler],
            // so we don't need Sentry's default one.
            options.enableUncaughtExceptionHandler = false
            // Disable uncaught native exceptions from being reported.
            // Sentry don't have a way to disable uncaught native exceptions from being reported.
            // As a fallback we had to disable all native integrations.
            // More info can be found https://github.com/getsentry/sentry-java/issues/1993
            options.isEnableNdk = false
            options.dsn = dsn
            options.environment = environment
        }
    }

    @VisibleForTesting
    internal fun prepareReport(
        breadcrumbs: ArrayList<MozillaBreadcrumb>,
        level: SentryLevel? = null
    ) {
        initIfNeeded()

        breadcrumbs.forEach {
            Sentry.addBreadcrumb(it.toSentryBreadcrumb())
        }

        level?.apply {
            Sentry.setLevel(level)
        }
    }

    private fun SentryId.alsoClearBreadcrumbs(): String {
        Sentry.clearBreadcrumbs()
        return this.toString()
    }

    @VisibleForTesting
    internal fun createMessage(crash: Crash.NativeCodeCrash): String {
        val fatal = crash.isFatal.toString()
        val processType = crash.processType
        val minidumpSuccess = crash.minidumpSuccess

        return "NativeCodeCrash(fatal=$fatal, processType=$processType, minidumpSuccess=$minidumpSuccess)"
    }
}

@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
internal fun MozillaBreadcrumb.toSentryBreadcrumb(): Breadcrumb {
    val sentryLevel = this.level.toSentryBreadcrumbLevel()
    val breadcrumb = Breadcrumb().apply {
        message = this@toSentryBreadcrumb.message
        category = this@toSentryBreadcrumb.category
        level = sentryLevel
        type = this@toSentryBreadcrumb.type.value
    }
    this.data.forEach {
        breadcrumb.setData(it.key, it.value)
    }
    return breadcrumb
}

@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
internal fun MozillaBreadcrumb.Level.toSentryBreadcrumbLevel() = when (this) {
    MozillaBreadcrumb.Level.CRITICAL -> SentryLevel.FATAL
    MozillaBreadcrumb.Level.ERROR -> SentryLevel.ERROR
    MozillaBreadcrumb.Level.WARNING -> SentryLevel.WARNING
    MozillaBreadcrumb.Level.INFO -> SentryLevel.INFO
    MozillaBreadcrumb.Level.DEBUG -> SentryLevel.DEBUG
}
