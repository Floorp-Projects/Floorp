/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.service

import android.content.Context
import mozilla.components.lib.crash.Crash
import org.mozilla.geckoview.CrashReporter
import java.io.File

typealias GeckoCrashReporter = CrashReporter

/**
 * A [CrashReporterService] implementation uploading crash reports to crash-stats.mozilla.com.
 *
 * @param applicationContext The application [Context].
 * @param appName A human-readable app name. This name is used on crash-stats.mozilla.com to filter crashes by app.
 *                The name needs to be whitelisted for the server to accept the crash.
 *                [File a bug](https://bugzilla.mozilla.org/enter_bug.cgi?product=Socorro) if you would like to get your
 *                app added to the whitelist.
 */
class MozillaSocorroService(
    private val applicationContext: Context,
    private val appName: String
) : CrashReporterService {
    override fun report(crash: Crash.UncaughtExceptionCrash) {
        // Not implemented currently.

        // In theory we could upload uncaught exception crashes to Socorro too. But Socorro is not the best tool for
        // them and it is not used by the app teams.
    }

    override fun report(crash: Crash.NativeCodeCrash) {
        // GeckoView comes with a crash reporter class that we can use here. For now we are assuming that we only want
        // to upload native code crashes to Socorro if GeckoView is used. If this assumption is going to be wrong in
        // the future then we can start inlining the functionality here.
        sendViaGeckoViewCrashReporter(crash)
    }

    internal fun sendViaGeckoViewCrashReporter(crash: Crash.NativeCodeCrash) {
        // GeckoView Nightly introduced a breaking API change to the crash reporter that has not been uplifted to beta.
        // Since our CrashReporter does not follow the same abstractions as the engine, this results in
        // a `NoSuchMethodError` being thrown.
        // We should fix this in the future to make the crash reporter be part of the same engine extraction.
        // See: https://github.com/mozilla-mobile/android-components/issues/4052
        try {
            GeckoCrashReporter.sendCrashReport(
                applicationContext,
                File(crash.minidumpPath),
                File(crash.extrasPath),
                appName
            )
        } catch (e: NoSuchMethodError) {
            val deprecatedMethod = GeckoCrashReporter::class.java.getDeclaredMethod(
                "sendCrashReport",
                Context::class.java,
                File::class.java,
                File::class.java,
                Boolean::class.java,
                String::class.java
            )
            deprecatedMethod.invoke(
                GeckoCrashReporter::class.java,
                applicationContext,
                File(crash.minidumpPath),
                File(crash.extrasPath),
                crash.minidumpSuccess,
                appName
            )
        }
    }
}
