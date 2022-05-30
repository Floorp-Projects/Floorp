/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus

import android.content.Context
import android.os.Build
import android.os.StrictMode
import androidx.appcompat.app.AppCompatDelegate
import androidx.lifecycle.ProcessLifecycleOwner
import androidx.preference.PreferenceManager
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.DelicateCoroutinesApi
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import mozilla.components.support.base.facts.register
import mozilla.components.support.base.log.Log
import mozilla.components.support.base.log.sink.AndroidLogSink
import mozilla.components.support.ktx.android.content.isMainProcess
import mozilla.components.support.locale.LocaleAwareApplication
import mozilla.components.support.rusthttp.RustHttpConfig
import mozilla.components.support.rustlog.RustLog
import mozilla.components.support.webextensions.WebExtensionSupport
import org.mozilla.focus.biometrics.LockObserver
import org.mozilla.focus.ext.settings
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

    @OptIn(DelicateCoroutinesApi::class)
    override fun onCreate() {
        super.onCreate()

        Log.addSink(AndroidLogSink("Focus"))
        components.crashReporter.install(this)

        if (isMainProcess()) {
            initializeNativeComponents()

            PreferenceManager.setDefaultValues(this, R.xml.settings, false)

            setTheme(this)
            components.engine.warmUp()

            TelemetryWrapper.init(this)
            components.metrics.initialize(this)
            FactsProcessor.initialize()
            ProfilerMarkerFactProcessor.create { components.engine.profiler }.register()

            enableStrictMode()

            AdjustHelper.setupAdjustIfNeeded(this@FocusApplication)

            visibilityLifeCycleCallback = VisibilityLifeCycleCallback(this@FocusApplication)
            registerActivityLifecycleCallbacks(visibilityLifeCycleCallback)
            registerComponentCallbacks(visibilityLifeCycleCallback)

            storeLink.start()

            GlobalScope.launch(Dispatchers.IO) {
                components.migrator.start(this@FocusApplication)
            }

            initializeWebExtensionSupport()

            setupLeakCanary()

            components.appStartReasonProvider.registerInAppOnCreate(this)
            components.startupActivityLog.registerInAppOnCreate(this)

            ProcessLifecycleOwner.get().lifecycle.addObserver(lockObserver)
        }
    }

    protected open fun setupLeakCanary() {
        // no-op, LeakCanary is disabled by default
    }

    open fun updateLeakCanaryState(isEnabled: Boolean) {
        // no-op, LeakCanary is disabled by default
    }

    @OptIn(DelicateCoroutinesApi::class) // GlobalScope usage
    private fun initializeNativeComponents() {
        GlobalScope.launch(Dispatchers.IO) {
            // We need to use an unwrapped client because native components do not support private
            // requests.
            @Suppress("Deprecation")
            RustHttpConfig.setClient(lazy { components.client.unwrap() })
            RustLog.enable(components.crashReporter)
            // We want to ensure Nimbus is initialized as early as possible so we can
            // experiment on features close to startup.
            // But we need viaduct (the RustHttp client) to be ready before we do.
            components.experiments.initialize()
        }
    }

    private fun setTheme(context: Context) {
        val settings = context.settings
        when {
            settings.lightThemeSelected -> {
                AppCompatDelegate.setDefaultNightMode(
                    AppCompatDelegate.MODE_NIGHT_NO
                )
            }

            settings.darkThemeSelected -> {
                AppCompatDelegate.setDefaultNightMode(
                    AppCompatDelegate.MODE_NIGHT_YES
                )
            }

            settings.useDefaultThemeSelected -> {
                setDefaultTheme()
            }

            // No theme setting selected, select the default value, follow device theme.
            else -> {
                setDefaultTheme()
                settings.useDefaultThemeSelected = true
            }
        }
    }

    private fun setDefaultTheme() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            AppCompatDelegate.setDefaultNightMode(
                AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM
            )
        } else {
            AppCompatDelegate.setDefaultNightMode(
                AppCompatDelegate.MODE_NIGHT_AUTO_BATTERY
            )
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
