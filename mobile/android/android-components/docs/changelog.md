---
layout: page
title: Changelog
permalink: /changelog/
---

# 0.22 (2018-09-07)

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.21...v0.22),
[Milestone](https://github.com/mozilla-mobile/android-components/milestone/22?closed=1),
[API reference](https://mozilla-mobile.github.io/android-components/api/0.22/index)

* Compiled against:
  * Android
    * SDK: 27
    * Support Libraries: 27.1.1
  * Kotlin
    * Standard library: 1.2.61
    * Coroutines: 0.23.4
  * GeckoView
    * Nightly: **64.0.20180905100117** 🔺
    * Beta: **63.0b3** (0269319281578bff4e01d77a21350bf91ba08620) 🔺
    * Release: **62.0** (9cbae12a3fff404ed2c12070ad475424d0ae869f) 🔺

* We now provide aggregated API docs. The docs for this release are hosted at: https://mozilla-mobile.github.io/android-components/api/0.22
* **browser-engine-***:
  * EngineView now exposes lifecycle methods with default implementations. A `LifecycleObserver` implementation is provided which forwards events to EngineView instances.
  ```Kotlin
  lifecycle.addObserver(EngineView.LifecycleObserver(view))
   ```
  * Added engine setting for blocking web fonts:
  ```Kotlin
  GeckoEngine(runtime, DefaultSettings(webFontsEnabled = false))
  ```
  * `setDesktopMode()` was renamed to `toggleDesktopMode()`.
* **browser-engine-system**: The `X-Requested-With` header is now cleared (set to an empty String).
* **browser-session**: Desktop mode can be observed now:
  ```Kotlin
  session.register(object : Session.Observer {
      fun onDesktopModeChange(enabled: Boolean) {
          // ..
      }
  })
  ```
* **service-fretboard**:
  * `Fretboard` now has synchronous methods for adding and clearing overrides: `setOverrideNow()`, `clearOverrideNow`, `clearAllOverridesNow`.
  * Access to `Experiment.id` is now deprecated and is scheduled to be removed in a future release (target: 0.24). The `id` is an implementation detail of the underlying storage service and was not meant to be exposed to apps.
* **ui-tabcounter**: Due to a packaging error previous releases of this component didn't contain any compiled code. This is the first usable release of the component.


# 0.21 (2018-08-31)

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.20...v0.21),
[Milestone](https://github.com/mozilla-mobile/android-components/milestone/21?closed=1),
[API reference](https://mozilla-mobile.github.io/android-components/api/0.21/index)

* Compiled against:
  * Android support libraries 27.1.1
  * Kotlin Standard library **1.2.61** 🔺
  * Kotlin coroutines 0.23.4
  * GeckoView
    * Nightly: **63.0.20180830111743** 🔺
    * Beta: **62.0b21** (7ce198bb7ce027d450af3f69a609896671adfab8) 🔺
    * Release: 61.0 (785d242a5b01d5f1094882aa2144d8e5e2791e06)

* **concept-engine**, **engine-system**, **engine-gecko**: Added API to set default session configuration e.g. to enable tracking protection for all sessions by default.
    ```Kotlin
    // DefaultSettings can be set on GeckoEngine and SystemEngine.
    GeckoEngine(runtime, DefaultSettings(
        trackingProtectionPolicy = TrackingProtectionPolicy.all(),
        javascriptEnabled = false))
    ```
* **concept-engine**, **engine-system**, **engine-gecko-beta/nightly**:
  * Added support for intercepting request and injecting custom content. This can be used for internal pages (e.g. *focus:about*, *firefox:home*) and error pages.
    ```Kotlin
    // GeckoEngine (beta/nightly) and SystemEngine support request interceptors.
    GeckoEngine(runtime, DefaultSettings(
        requestInterceptor = object : RequestInterceptor {
            override fun onLoadRequest(session: EngineSession, uri: String): RequestInterceptor.InterceptionResponse? {
                return when (uri) {
                    "sample:about" -> RequestInterceptor.InterceptionResponse("<h1>I am the sample browser</h1>")
                    else -> null
               }
           }
       }
    )
    ```
  * Added APIs to support "find in page".
    ```Kotlin
        // Finds and highlights all occurrences of "hello"
        engineSession.findAll("hello")

        // Finds and highlights the next or previous match
        engineSession.findNext(forward = true)

        // Clears the highlighted results
        engineSession.clearFindMatches()

        // The current state of "Find in page" can be observed on a Session object:
        session.register(object : Session.Observer {
            fun onFindResult(session: Session, result: FindResult) {
                // ...
            }
        })
    ```
* **browser-engine-gecko-nightly**: Added option to enable/disable desktop mode ("Request desktop site").
    ```Kotlin
        engineSession.setDesktopMode(true, reload = true)
    ```
* **browser-engine-gecko(-nightly/beta)**: Added API for observing long presses on web content (links, audio, videos, images, phone numbers, geo locations, email addresses).
    ```Kotlin
        session.register(object : Session.Observer {
            fun onLongPress(session: Session, hitResult: HitResult): Boolean {
                // HitResult is a sealed class representing the different types of content that can be long pressed.
                // ...

                // Returning true will "consume" the event. If no observer consumes the event then it will be
                // set on the Session object to be consumed at a later time.
                return true
            }
        })
    ```
* **lib-dataprotect**: New component to protect local user data using the [Android keystore system](https://developer.android.com/training/articles/keystore). This component doesn't contain any code in this release. In the next sprints the Lockbox team will move code from the [prototype implementation](https://github.com/linuxwolf/android-dataprotect) to the component.
* **support-testing**: New helper test function to assert that a code block throws an exception:
    ```Kotlin
    expectException(IllegalStateException::class) {
        // Do something that should throw IllegalStateException..
    }
    ```

# 0.20 (2018-08-24)

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.19.1...v0.20),
[Milestone](https://github.com/mozilla-mobile/android-components/milestone/19?closed=1),
[API reference](https://mozilla-mobile.github.io/android-components/api/0.20/index)

* Compiled against:
  * Android support libraries 27.1.1
  * Kotlin Standard library 1.2.60
  * Kotlin coroutines 0.23.4
  * GeckoView
    * Nightly: **63.0.20180820100132** 🔺
    * Beta: 62.0b15 (7ce198bb7ce027d450af3f69a609896671adfab8)
    * Release: 61.0 (785d242a5b01d5f1094882aa2144d8e5e2791e06)

* GeckoView Nightly dependencies are now pulled in from *maven.mozilla.org*.
* **engine-system**: Added tracking protection functionality.
* **concept-engine**, **browser-session**, **feature-session**: Added support for private browsing mode.
* **concept-engine**, **engine-gecko**, **engine-system**: Added support for modifying engine and engine session settings.

# 0.19.1 (2018-08-20)

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.19...v0.19.1),
[Milestone](https://github.com/mozilla-mobile/android-components/milestone/20?closed=1),
[API reference](https://mozilla-mobile.github.io/android-components/api/0.19.1/index)

* Compiled against:
  * Android support libraries 27.1.1
  * Kotlin Standard library 1.2.60
  * Kotlin coroutines 0.23.4
  * GeckoView
    * Nightly: 63.0.20180810100129 (2018.08.10, d999fb858fb2c007c5be4af72bce419c63c69b8e)
    * Beta: 62.0b15 (7ce198bb7ce027d450af3f69a609896671adfab8)
    * Release: 61.0 (785d242a5b01d5f1094882aa2144d8e5e2791e06)

* **browser-toolbar**: Replaced `ui-progress` component with default [Android Progress Bar](https://developer.android.com/reference/android/widget/ProgressBar) to fix CPU usage problems.
* **ui-progress**: Reduced high CPU usage when idling and not animating.


# 0.19 (2018-08-17)

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.18...v0.19),
[Milestone](https://github.com/mozilla-mobile/android-components/milestone/18?closed=1),
[API reference](https://mozilla-mobile.github.io/android-components/api/0.19/index)

* Compiled against:
  * Android support libraries 27.1.1
  * Kotlin Standard library 1.2.60
  * Kotlin coroutines 0.23.4
  * GeckoView
    * Nightly: 63.0.20180810100129 (2018.08.10, d999fb858fb2c007c5be4af72bce419c63c69b8e)
    * Beta: 62.0b15 (7ce198bb7ce027d450af3f69a609896671adfab8)
    * Release: 61.0 (785d242a5b01d5f1094882aa2144d8e5e2791e06)

* **concept-engine**, **engine-system**, **engine-gecko**: Added new API to load data and HTML directly (without loading a URL). Added the ability to stop loading a page.
* **ui-autocomplete**: Fixed a bug that caused soft keyboards and the InlineAutocompleteEditText component to desync.
* **service-firefox-accounts**: Added JNA-specific proguard rules so consumers of this library don't have to add them to their app (see https://github.com/java-native-access/jna/blob/master/www/FrequentlyAskedQuestions.md#jna-on-android for details). Underlying libfxa_client.so no longer depends on versioned .so names. All required dependencies are now statically linked which simplified our dependency setup as well.

# 0.18 (2018-08-10)

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.17...v0.18),
[Milestone](https://github.com/mozilla-mobile/android-components/milestone/16?closed=1),
[API reference](https://mozilla-mobile.github.io/android-components/api/0.18/index)

* Compiled against:
  * Android support libraries 27.1.1
  * Kotlin Standard library 1.2.60
  * Kotlin coroutines 0.23.4
  * GeckoView
    * Nightly: **63.0.20180810100129** (2018.08.10, d999fb858fb2c007c5be4af72bce419c63c69b8e) 🔺
    * Beta: **62.0b15** (7ce198bb7ce027d450af3f69a609896671adfab8) 🔺
    * Release: 61.0 (785d242a5b01d5f1094882aa2144d8e5e2791e06)

* **engine-gecko-beta**: Since the [Load Progress Tracking API](https://bugzilla.mozilla.org/show_bug.cgi?id=1437988) was uplifted to GeckoView Beta  _engine-gecko-beta_ now reports progress via `EngineSession.Observer.onProgress()`.
* **service-fretboard**: KintoExperimentSource can now validate the signature of the downloaded experiments configuration (`validateSignature` flag). This ensures that the configuration was signed by Mozilla and was not modified by a bad actor. For now the `validateSignature` flag is off by default until this has been tested in production. Various bugfixes and refactorings.
* **service-firefox-accounts**: JNA native libraries are no longer part of the AAR and instead referenced as a dependency. This avoids duplication when multiple libraries depend on JNA.
* **ui-tabcounter**: New UI component - A button that shows the current tab count and can animate state changes. Extracted from Firefox Rocket.
* API references for every release are now generated and hosted online: [https://mozilla-mobile.github.io/android-components/reference/](https://mozilla-mobile.github.io/android-components/reference/)
* Documentation and more is now hosted at: [https://mozilla-mobile.github.io/android-components/](https://mozilla-mobile.github.io/android-components/). More content coming soon.
* **tooling-lint**: New (internal-only) component containing custom lint rules.


# 0.17 (2018-08-03)

* Compiled against:
  * Android support libraries 27.1.1
  * Kotlin Standard library **1.2.60** 🔺
  * Kotlin coroutines 0.23.4
  * GeckoView
    * Nightly: **63.0.20180801100114** (2018.08.01, af6a7edf0069549543f2fba6a8ee3ea251b20829) 🔺
    * Beta: **62.0b13** (dd92dec96711e60a8c6a49ebe584fa23a453a292) 🔺
    * Release: 61.0 (785d242a5b01d5f1094882aa2144d8e5e2791e06)

* **support-base**: New base component containing small building blocks for other components. Added a [simple logging API](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/README.md) that allows components to log messages/exceptions but lets the consuming app decide what gets logged and how.
* **support-utils**: Some classes have been moved to the new _support-base_ component.
* **service-fretboard**: ⚠️ Breaking change: `ExperimentDescriptor` instances now operate on the experiment name instead of the ID.
* **ui-icons**: Added new icons (used in _Firefox Focus_ UI refresh): `mozac_ic_arrowhead_down`, `mozac_ic_arrowhead_up`, `mozac_ic_check`, `mozac_ic_device_desktop`, `mozac_ic_mozilla`, `mozac_ic_open_in`, `mozac_ic_reorder`.
* **service-firefox-accounts**: Added [documentation](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/README.md).
* **service-fretboard**: Updated [documentation](https://github.com/mozilla-mobile/android-components/blob/master/components/service/fretboard/README.md).
* **browser-toolbar**: Fixed an issue where the toolbar content disappeared if a padding value was set on the toolbar.

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.16.1...v0.17), [Milestone](https://github.com/mozilla-mobile/android-components/milestone/15?closed=1)

# 0.16.1 (2018-07-26)

* Compiled against:
  * Android support libraries 27.1.1
  * Kotlin Standard library 1.2.51
  * Kotlin coroutines 0.23.4
  * GeckoView
    * Nightly: 63.0.20180724100046 (2018.07.24, 1e5fa52a612e8985e12212d1950a732954e00e45)
    * Beta: 62.0b9 (d7ab2f3df0840cdb8557659afd46f61afa310379)
    * Release: 61.0 (785d242a5b01d5f1094882aa2144d8e5e2791e06)

* **service-telemetry**: Allow up to 200 extras in event pings.

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.16...v0.16.1), [Milestone](https://github.com/mozilla-mobile/android-components/milestone/17?closed=1)

# 0.16 (2018-07-25)

* Compiled against:
  * Android support libraries 27.1.1
  * Kotlin Standard library 1.2.51
  * Kotlin coroutines 0.23.4
  * GeckoView
    * Nightly: 63.0.20180724100046 (2018.07.24, 1e5fa52a612e8985e12212d1950a732954e00e45)
    * Beta: 62.0b9 (d7ab2f3df0840cdb8557659afd46f61afa310379)
    * Release: 61.0 (785d242a5b01d5f1094882aa2144d8e5e2791e06)

* **service-fretboard**: Experiments can now be filtered by release channel. Added helper method to get list of active experiments.
* **service-telemetry**: Added option to report active experiments in the core ping.
* **service-firefox-accounts**, **sample-firefox-accounts**: libjnidispatch.so is no longer in the tree but automatically fetched from tagged GitHub releases at build-time. Upgraded to fxa-rust-client library 0.2.1. Renmaed armeabi directory to armeabi-v7a.
* **browser-session**, **concept-engine**: Exposed website title and tracking protection in session and made observable.
* **browser-toolbar**: Fixed bug that prevented the toolbar from being displayed at the bottom of the screen. Fixed animation problem when multiple buttons animated at the same time.
* Various bugfixes and refactorings (see commits below for details)
* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.15...v0.16), [Milestone](https://github.com/mozilla-mobile/android-components/milestone/14?closed=1)

# 0.15 (2018-07-20)

* Compiled against:
  * Android support libraries 27.1.1
  * Kotlin Standard library 1.2.51
  * Kotlin coroutines 0.23.4
  * GeckoView
    * Nightly: 63.0.20180704100138 (2018.07.04, 1c235a552c32ba6c97e6030c497c49f72c7d48a8)
    * Beta: 62.0b5 (801112336847960bbb9a018695cf09ea437dc137)
    * Release: 61.0 (785d242a5b01d5f1094882aa2144d8e5e2791e06)

* **service-firefox-accounts**, **sample-firefox-accounts**: Added authentication flow using WebView. Introduced functionality to persist and restore FxA state in shared preferences to keep users signed in between applications restarts. Increased test coverage for library.
* **service-fretboard**: New component for segmenting users in order to run A/B tests and rollout features gradually.
* **browser-session**: Refactored session observer to provide session object and changed values to simplify observer implementations. Add source (origin) information to Session.
* **browser-search**: Introduced new functionality to retrieve search suggestions.
* **engine-system**, **engine-gecko**, **browser-session**: Exposed downloads in engine components and made them consumable from browser session.
* **engine-gecko**: Added optimization to ignore initial loads of about:blank.

* Various bugfixes and refactorings (see commits below for details)
* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.14...v0.15), [Milestone](https://github.com/mozilla-mobile/android-components/milestone/13?closed=1)

# 0.14 (2018-07-13)

* Compiled against:
  * Android support libraries 27.1.1
  * Kotlin Standard library 1.2.51
  * Kotlin coroutines 0.23.4
  * GeckoView
    * Nightly: 63.0.20180704100138 (2018.07.04, 1c235a552c32ba6c97e6030c497c49f72c7d48a8)
    * Beta: 62.0b5 (801112336847960bbb9a018695cf09ea437dc137)
    * Release: 61.0 (785d242a5b01d5f1094882aa2144d8e5e2791e06)

* **support-test**: A new component with helpers for testing components.
* **browser-session**: New method `SessionManager.removeSessions()` for removing all sessions *except custom tab sessions*. `SessionManager.selectedSession` is now nullable. `SessionManager.selectedSessionOrThrow` can be used in apps that will always have at least one selected session and that do not want to deal with a nullable type.
* **feature-sessions**: `SessionIntentProcessor` can now be configured to open new tabs for incoming [Intents](https://developer.android.com/reference/android/content/Intent).
* **ui-icons**: Mirrored `mozac_ic_pin` and `mozac_ic_pin_filled` icons.
* **service-firefox-accounts**: Renamed the component from *service-fxa* for clarity. Introduced `FxaResult.whenComplete()` to be called when the `FxaResult` and the whole chain of `then` calls is completed with a value. Synchronized blocks invoking Rust calls.
* Various bugfixes and refactorings (see commits below for details)
* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.13...v0.14), [Milestone](https://github.com/mozilla-mobile/android-components/milestone/12?closed=1)

# 0.13 (2018-07-06)

* Compiled against:
  * Android support libraries 27.1.1
  * Kotlin Standard library 1.2.51
  * Kotlin coroutines 0.23.4
  * GeckoView
    * Nightly: 63.0.20180704100138 (2018.07.04, 1c235a552c32ba6c97e6030c497c49f72c7d48a8)
    * Beta: 62.0b5
    * Release: 61.0

* **service-fxa**, **samples-fxa**: Various improvements to FxA component API (made calls asynchronous and introduced error handling)
* **browser-toolbar**: Added functionality to observer focus changes (`setOnEditFocusChangeListener`)
* **concept-tabstray**, **browser-tabstray**, **features-tabs**: New components to provide browser tabs functionality
* **sample-browser**: Updated to support multiple tabs

* **API changes**:
  * InlineAutocompleteEditText: `onAutocomplete` was renamed to `applyAutocompleteResult`
  * Toolbar: `setOnUrlChangeListener` was renamed to `setOnUrlCommitListener`

* Various bugfixes and refactorings (see commits below for details)
* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.12...v0.13)

# 0.12 (2018-06-29)

* Compiled against:
  * Android support libraries 27.1.1
  * Kotlin Standard library 1.2.50
  * Kotlin coroutines 0.23.3
  * GeckoView Nightly
    * date: 2018.06.27
    * version: 63.0.20180627100018
    * revision: 1c235a552c32ba6c97e6030c497c49f72c7d48a8

* **service-fxa**, **samples-fxa**: Added new library/component for integrating with Firefox Accounts, and a sample app to demo its usage
* **samples-browser**: Moved all browser behaviour into standalone fragment
* Various bugfixes and refactorings (see commits below for details)

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.11...v0.12)

# 0.11 (2018-06-22)

* Compiled against:
  * Android support libraries 27.1.1
  * Kotlin Standard library 1.2.41
  * Kotlin coroutines 0.22.5
  * GeckoView Nightly
    * date: 2018.06.21
    * version: 62.0.20180621100051
    * revision: e834d23a292972ab4250a8be00e6740c43e41db2

* **feature-session**, **browser-session**: Added functionality to process CustomTabsIntent.
* **engine-gecko**: Created separate engine-gecko variants/modules for nightly/beta/release channels.
* **browser-toolbar**: Added support for setting autocomplete filter.
* Various refactorings (see commits below for details)

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.10...v0.11)

# 0.10 (2018-06-14)

* Compiled against:
  * Android support libraries 27.1.1
  * Kotlin Standard library 1.2.41
  * Kotlin coroutines 0.22.5
  * GeckoView Nightly
    * date: 2018.05.16
    * version: 62.0.20180516100458
    * revision: dedd25bfd2794eaba95225361f82c701e49c9339

* **browser-session**: Added Custom Tabs configuration to session. Added new functionality that allows attaching a lifecycle owner to session observers so that observer can automatically be unregistered when the associated lifecycle ends.
* **service-telemetry**: Updated createdTimestamp and createdDate fields for mobile-metrics ping

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.9...v0.10)

# 0.9 (2018-06-06)

* Compiled against:
  * Android support libraries 27.1.1
  * Kotlin Standard library 1.2.41
  * Kotlin coroutines 0.22.5
  * GeckoView Nightly
    * date: 2018.05.16
    * version: 62.0.20180516100458
    * revision: dedd25bfd2794eaba95225361f82c701e49c9339

* **feature-session**, **engine-gecko**, **engine-system**: Added functionality and API to save/restore engine session state and made sure it's persisted by default (using `DefaultSessionStorage`)
* **concept-toolbar**: Use "AppCompat" versions of ImageButton and ImageView. Add `notifyListener` parameter to `setSelected` and `toggle` to specify whether or not listeners should be invoked.

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.8...v0.9)

# 0.8 (2018-05-30)

* Compiled against:
  * Android support libraries 27.1.1
  * Kotlin Standard library 1.2.41
  * Kotlin coroutines 0.22.5
  * GeckoView Nightly
    * date: 2018.05.16
    * version: 62.0.20180516100458
    * revision: dedd25bfd2794eaba95225361f82c701e49c9339

* **browser-session**, **engine-gecko**, **engine-system**: Added SSL information and secure state to session, and made it observable.
* **browser-toolbar**: Introduced page, browser and navigation actions and allow for them to be dynamically shown, hidden and updated. Added ability to specify custom behaviour for clicks on URL in display mode. Added support for custom background actions. Enabled layout transitions by default.
* **service-telemetry**: Added new mobile-metrics ping type.

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.7...v0.8)

# 0.7 (2018-05-24)

* Compiled against:
  * Android support libraries 27.1.1
  * Kotlin Standard library 1.2.41
  * Kotlin coroutines 0.22.5
  * GeckoView Nightly
    * date: 2018.05.16
    * version: 62.0.20180516100458
    * revision: dedd25bfd2794eaba95225361f82c701e49c9339

* **browser-toolbar**: Added support for dynamic actions. Made site security indicator optional. Added support for overriding default height and padding.
* **feature-session**: Added new use case implementation to support reloading URLs. Fixed bugs when restoring sessions from storage. Use `AtomicFile` for `DefaultSessionStorage`.
* **feature-search**: New component - Connects an (concept) engine implementation with the browser search module and provides search related use case implementations e.g. searching using the default provider.
* **support-ktx**: Added extension method to check if a `String` represents a URL.
* **samples-browser**: Added default search integration using the new feature-search component.
* **samples-toolbar**: New sample app - Shows how to customize the browser-toolbar component.

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.6...v0.7)

# 0.6 (2018-05-16)

* Compiled against:
  * Android support libraries 27.1.1
  * Kotlin Standard library 1.2.41
  * Kotlin coroutines 0.22.5
  * GeckoView Nightly
    * date: 2018.05.16
    * version: 62.0.20180516100458
    * revision: dedd25bfd2794eaba95225361f82c701e49c9339

* **browser-menu**: New component - A generic menu with customizable items for browser toolbars.
* **concept-session-storage**: New component - Abstraction layer for hiding the actual session storage implementation.
* **feature-session**: Added `DefaultSessionStorage` which is used if no other implementation of `SessionStorage` (from the new concept module) is provided. Introduced a new `SessionProvider` type which simplifies the API for use cases and components and removed the `SessionMapping` type as it's no longer needed.
* **support-ktx**: Added extension methods to `View` for checking visibility (`View.isVisible`, `View.isInvisible` and `View.isGone`).
* **samples-browser**: Use new browser menu component and switch to Gecko as default engine.

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.5.1...v0.6)

# 0.5.1 (2018-05-03)

* Compiled against:
  * Android support libraries 27.1.1
  * Kotlin Standard library 1.2.41
  * Kotlin coroutines 0.22.5
  * GeckoView Nightly
    * date: 2018.04.10
    * version: 61.0.20180410100334
    * revision: a8061a09cd7064a8783ca9e67979d77fb52e001e

* **browser-domains**: Simplified API of `DomainAutoCompleteProvider` which now uses a dedicated result type instead of a callback and typealias.
* **browser-toolbar**: Added various enhancements to support edit and display mode and to navigate back/forward.
* **feature-session**: Added `SessionIntentProcessor` which provides reuseable functionality to handle incoming intents.
* **sample-browser**: Sample application now handles the device back button and reacts to incoming (ACTION_VIEW) intents.
* **support-ktx**: Added extension methods to `View` for converting dp to pixels (`View.dp`), showing and hiding the keyboard (`View.showKeyboard` and `View.hideKeyboard`).
* **service-telemetry**: New component - A generic library for generating and sending telemetry pings from Android applications to Mozilla's telemetry service.
* **ui-icons**: New component - A collection of often used browser icons.
* **ui-progress**: New component - An animated progress bar following the Photon Design System.

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.4...v0.5)

# 0.5 (2018-05-02)

_Due to a packaging bug this release is not usable. Please use 0.5.1 instead._

# 0.4 (2018-04-19)

* Compiled against:
  * Android support libraries 27.1.1
  * Kotlin Standard library 1.2.31
  * Kotlin coroutines 0.22.5

* **browser-search**: New module - Search plugins and companion code to load, parse and use them.
* **browser-domains**: Auto-completion of full URLs (instead of just domains) is now supported.
* **ui-colors** module (org.mozilla.photon:colors) now includes all photon colors.
* **ui-fonts**: New module - Convenience accessor for fonts used by Mozilla.
* Multiple (Java/Kotlin) package names have been changed to match the naming of the module. Module names usually follow the template "$group-$name" and package names now follow the same scheme: "mozilla.components.$group.$name". For example the code of the "browser-toolbar" module now lives in the "mozilla.components.browser.toolbar" package. The group and artifacts Ids in Maven/Gradle have not been changed at this time.

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.3...v0.4)

# 0.3 (2018-04-05)

* Compiled against:
  * Android support libraries 27.1.1
  * Kotlin Standard library 1.2.30
  * Kotlin coroutines 0.19.3

* New component: **ui-autocomplete** - A set of components to provide autocomplete functionality. **InlineAutocompleteEditText** is a Kotlin version of the inline autocomplete widget we have been using in Firefox for Android and Focus/Klar for Android.
* New component: **browser-domains** - Localized and customizable domain lists for auto-completion in browsers.
* New components (Planning phase; Not for consumption yet): engine, engine-gecko, session, toolbar

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.2.2...v0.3)

# 0.2.2 (2018-03-27)

* Compiled against:
  * Android support libraries 27.1.0
  * Kotlin Standard library 1.2.30

* First release with synchronized version numbers.

