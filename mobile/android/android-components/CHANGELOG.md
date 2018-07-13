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

