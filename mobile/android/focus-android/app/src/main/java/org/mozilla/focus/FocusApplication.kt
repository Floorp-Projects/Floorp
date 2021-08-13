/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus

import android.os.StrictMode
import androidx.lifecycle.ProcessLifecycleOwner
import androidx.preference.PreferenceManager
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import mozilla.components.support.base.facts.register
import mozilla.components.support.base.log.Log
import mozilla.components.support.base.log.sink.AndroidLogSink
import mozilla.components.support.ktx.android.content.isMainProcess
import mozilla.components.support.webextensions.WebExtensionSupport
import org.mozilla.focus.biometrics.LockObserver
import org.mozilla.focus.locale.LocaleAwareApplication
import org.mozilla.focus.navigation.StoreLink
import org.mozilla.focus.session.VisibilityLifeCycleCallback
import org.mozilla.focus.telemetry.FactsProcessor
import org.mozilla.focus.telemetry.ProfilerMarkerFactProcessor
import org.mozilla.focus.telemetry.TelemetryWrapper
import org.mozilla.focus.utils.AdjustHelper
import org.mozilla.focus.utils.AppConstants
import kotlin.coroutines.CoroutineContext

open class FocusApplication : LocaleAwareApplication(), CoroutineScope {
    private var job = Job()
    override val coroutineContext: CoroutineContext
        get() = job + Dispatchers.Main

    open val components: Components by lazy { Components(this) }

    var visibilityLifeCycleCallback: VisibilityLifeCycleCallback? = null
        private set

    private val storeLink by lazy { StoreLink(components.appStore, components.store) }
    private val lockObserver by lazy { LockObserver(this, components.store, components.appStore) }

    override fun onCreate() {
        super.onCreate()

        Log.addSink(AndroidLogSink("Focus"))
        components.crashReporter.install(this)

        if (isMainProcess()) {
            PreferenceManager.setDefaultValues(this, R.xml.settings, false)

            components.engine.warmUp()

            TelemetryWrapper.init(this)
            components.metrics.initialize(this)
            FactsProcessor.initialize()
            ProfilerMarkerFactProcessor.create { components.engine.profiler }.register()

            enableStrictMode()

            AdjustHelper.setupAdjustIfNeeded(this@FocusApplication)

            visibilityLifeCycleCallback = VisibilityLifeCycleCallback(this@FocusApplication)
            registerActivityLifecycleCallbacks(visibilityLifeCycleCallback)

            storeLink.start()

            initializeWebExtensionSupport()

            ProcessLifecycleOwner.get().lifecycle.addObserver(lockObserver)
        }
    }

    private fun enableStrictMode() {
        // Android/WebView sometimes commit strict mode violations, see e.g.
        // https://github.com/mozilla-mobile/focus-android/issues/660
        if (AppConstants.isReleaseBuild || AppConstants.isBetaBuild) {
            return
        }

        val threadPolicyBuilder = StrictMode.ThreadPolicy.Builder().detectAll()
        val vmPolicyBuilder = StrictMode.VmPolicy.Builder()
                .detectActivityLeaks()
                .detectFileUriExposure()
                .detectLeakedClosableObjects()
                .detectLeakedRegistrationObjects()
                .detectLeakedSqlLiteObjects()

        threadPolicyBuilder.penaltyLog()
        vmPolicyBuilder.penaltyLog()

        StrictMode.setThreadPolicy(threadPolicyBuilder.build())
        StrictMode.setVmPolicy(vmPolicyBuilder.build())
    }

    private fun initializeWebExtensionSupport() {
        WebExtensionSupport.initialize(
            components.engine,
            components.store,
            onNewTabOverride = { _, engineSession, url ->
                components.tabsUseCases.addTab(
                    url = url,
                    selectTab = true,
                    engineSession = engineSession,
                    private = true
                )
            }
        )
    }
}
