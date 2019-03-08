---
layout: page
title: Changelog
permalink: /changelog/
---

# 0.46.0-SNAPSHOT  (In Development)

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.45.0...master)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/49?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/master/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/master/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/master/buildSrc/src/main/java/Config.kt)

* **browser-awesomebar**
  * Adds ability to remove `SuggestionProvider`s with `removeProviders` and `removeAllProviders`

* **browser-menu**
  * ⚠️ **This is a breaking API change!**: Removed redundant `BrowserMenuImageText` `contentDescription`

* **feature-downloads**
  * Fixing bug #2265. In some occasions, when trying to download a file, the download failed and the download notification shows "Unsuccessful download".

* **service-experiments**
  * A new client-side experiments SDK for running segmenting user populations to run multi-branch experiments on them. This component is going to replace `service-fretboard`. The SDK is currently in development and the component is not ready to be used yet.

# 0.45.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.44.0...v0.45.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/47?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v0.45.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v0.45.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v0.45.0/buildSrc/src/main/java/Config.kt)

* Mozilla App Services dependency upgraded: **0.18.0** 🔺
  * [0.18.0 release notes](https://github.com/mozilla/application-services/releases/tag/v0.18.0)

* **browser-engine-gecko-nightly**
  * Added API to install web extensions:

  ```kotlin
  val borderify = WebExtension("borderify", "resource://android/assets/extensions/borderify/")
  engine.installWebExtension(borderify) {
      ext, throwable -> Log.log(Log.Priority.ERROR, "MyApp", throwable, "Failed to install ${ext.id}")
  }
  ```

* **feature-search**
  * Added `newPrivateTabSearch` `NewTabSearchUseCase`

* **feature-toolbar**
  * Added ability to color parts of the domain (e.g. [registrable domain](https://url.spec.whatwg.org/#host-registrable-domain)) by providing a `UrlRenderConfiguration`:

  ```kotlin
  ToolbarFeature(
    // ...
    ToolbarFeature.UrlRenderConfiguration(
        publicSuffixList, // Use a shared global instance
        registrableDomainColor = 0xFFFF0000.toInt(),
        urlColor = 0xFF00FF00.toInt()
    )
  ```

* **browser-toolbar**
  * `BrowserToolbar` `cancelView` is now `clearView` with new text clearing behavior and color attribute updated from `browserToolbarCancelColor` to `browserToolbarClearColor`

* **concept-awesomebar**
  * ⚠️ **This is a breaking API change**: [AwesomeBar.Suggestion](https://mozac.org/api/mozilla.components.concept.awesomebar/-awesome-bar/-suggestion/) instances must now declare the provider that created them.

* **browser-awesomebar**
  * [BrowserAwesomeBar](https://mozac.org/api/mozilla.components.browser.awesomebar/-browser-awesome-bar/) is now replacing suggestions "in-place" if their ids match. Additionally `BrowserAwesomeBar` now automatically scrolls to the top whenever the entered text changes.
  
* **feature-customtabs**
  * Now returns false in `onBackPressed()` if feature is not initialized

* **support-android-test**
  * 🆕 New component to be used for helpers used in instrumented (on device) tests (`src/androidTest`). This component complements `support-test` which is focused on helpers used in local unit tests (`src/test`).
  * Added helper `LiveData.awaitValue()` which subscribes to the `LiveData` object and blocks until a value was observed. Returns the value or throws an `InterruptedException` if no value was observed (customizable timeout).

* **feature-session-bundling**
  * Added optional `since` parameter to `SessionBundleStorage.bundles()` and `SessionBundleStorage.bundlesPaged()`.

# 0.44.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.43.0...v0.44.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/46?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v0.44.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v0.44.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v0.44.0/buildSrc/src/main/java/Config.kt)

* **browser-menu**
  * Added option to set background color by overriding `mozac_browser_menu_background` color resource.
  
    ```xml
    <color name="mozac_browser_menu_background">DESIRED_COLOR</color>
    ```
    **OR**
      ```xml
      <style name="Mozac.Browser.Menu" parent="" tools:ignore="UnusedResources">
        <item name="cardBackgroundColor">YOUR_COLOR</item>
      </style>
    ```
    
  * Added option to style `SimpleBrowserMenuItem` and `BrowserMenuImageText` with `textColorResource`.

* **browser-toolbar**
  * Added option to configure fading edge length by using `browserToolbarFadingEdgeSize` XML attribute.
  * Added `BrowserToolbar` attribute `browserToolbarCancelColor` to color the cancel icon.

* **feature-toolbar**
  * `ToolbarPresenter` now handles situations where no `Session` is selected.

* **intent-processor**
  * Intent processor now lets you set `isPrivate` which will open process intents as private tabs

* **service-fretboard (Kinto)**
  * ⚠️ **This is a breaking API change!**
  * Now makes use of our concept-fetch module when communicating with the server. This allows applications to specify which HTTP client library to use e.g. apps already using GeckoView can now specify that the `GeckoViewFetchClient` should be used. As a consequence, the fetch client instance now needs to be provided when creating a `KintoExperimentSource`. 

  ```kotlin
    val fretboard = Fretboard(    
      KintoExperimentSource(
        baseUrl,
        bucketName,
        collectionName,
        // Specify that the GV-based fetch client should be used.
        GeckoViewFetchClient(context)
      ),
      experimentStorage
  )  
  ```

* **feature-session-bundling**
  * Added `SessionBundleStorage.autoClose()`: When "auto close" is enabled the currently active `SessionBundle` will automatically be closed and a new `SessionBundle`  will be started if the bundle lifetime expires while the app is in the background.

* **browser-engine-gecko**, **browser-engine-gecko-beta**, **browser-engine-gecko-nightly**:
  * Fixed an issue that caused [autofill](https://developer.android.com/guide/topics/text/autofill) to not work with those components.

* **feature-sitepermissions**
  * 🆕 A feature for showing site permission request prompts. For more info take a look at the [docs](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/sitepermissions/README.md).

* **browser-session**
  * Added `SelectionAwareSessionObserver.observeIdOrSelected(sessionId: String?)` to observe the session based on a session ID. If the session does not exist, then observe the selected session.

# 0.43.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.42.0...v0.43.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/45?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v0.43.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v0.43.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v0.43.0/buildSrc/src/main/java/Config.kt)

* **browser-engine-system**
  * Added support for [JavaScript confirm alerts](https://developer.mozilla.org/en-US/docs/Web/API/Window/confirm) on WebView.

* **browser-icons**
  * 🆕 New component for loading and storing website icons (like [Favicons](https://en.wikipedia.org/wiki/Favicon)).
  * Supports generating a "fallback" icon if no icon could be loaded.

* **concept-fetch**
  * Added API to specify whether or not cookies should be sent with a request. This can be controlled using the `cookiePolicy` parameter when creating a `Request`.

  ```kotlin
  // Do not send cookies with this request
  client.fetch(Request(url, cookiePolicy = CookiePolicy.OMIT)).use { response ->
    val body = response.body.string()
  }
  ```
  * Added flag to specify whether or not caches should be used. This can be controlled with the `useCaches` parameter when creating a `Request`.

  ```kotlin
  // Force a network request (do not use cached responses)
  client.fetch(Request(url, useCaches = false)).use { response ->
    val body = response.body.string()
  }
  ```

* **feature-awesomebar**
  * ⚠️ **This is a breaking API change!**
  * Now makes use of our concept-fetch module when fetching search suggestions. This allows applications to specify which HTTP client library to use e.g. apps already using GeckoView can now specify that the `GeckoViewFetchClient` should be used. As a consequence, the fetch client instance now needs to be provided when adding a search provider. 

  ```kotlin
  AwesomeBarFeature(layout.awesomeBar, layout.toolbar, layout.engineView)
    .addHistoryProvider(components.historyStorage, components.sessionUseCases.loadUrl)
    .addSessionProvider(components.sessionManager, components.tabsUseCases.selectTab)
    .addSearchProvider(
      components.searchEngineManager.getDefaultSearchEngine(requireContext()),
      components.searchUseCases.defaultSearch,
      // Specify that the GV-based fetch client should be used.
      GeckoViewFetchClient(context))
  ```

* **ui-doorhanger**
  * Added `DoorhangerPrompt` - a builder for creating a prompt `Doorhanger` providing a way to present decisions to users.

* **feature-downloads**
  * Ignoring schemes that are not https or http [#issue 554](https://github.com/mozilla-mobile/reference-browser/issues/554)

* **support-ktx**
  * Added `Uri.hostWithoutCommonPrefixes` to return the host with common prefixes removed:

  ```kotlin
  "https://www.mozilla.org"
      .toUri()
      .hostWithoutCommonPrefixes // mozilla.org

  "https://mobile.twitter.com/home"
      .toUri()
      .hostWithoutCommonPrefixes  // twitter.com

  "https://m.facebook.com/"
      .toUri()
      .hostWithoutCommonPrefixes
  ```

  ℹ️ Note that this method only strips common prefixes like "www", "m" or "mobile". If you are interested in extracting something like the [eTLD](https://en.wikipedia.org/wiki/Public_Suffix_List) from a host then use [PublicSuffixList](https://mozac.org/api/mozilla.components.lib.publicsuffixlist/-public-suffix-list/) of the `lib-publicsuffixlist` component.

  * Added `String.toUri()` as a shorthand for `Uri.parse()` and in addition to other `to*()` methods already available in the Kotlin Standard Library.

* **support-utils**
  * Added `Browsers` utility class for collecting and analyzing information about installed browser applications.

* **browser-session**, **feature-session-bundling**
  * `SessionStorage` and `SessionBundleStorage` now save and restore the title of `Session` objects.
  * `SessionManager.restore()` now allows passing in empty snapshots.

* **feature-session-bundling**
  * Empty snapshots are no longer saved in the database:
    * If no restored bundle exists then no new bundle is saved for an empty snapshot.
    * If there is an active bundle then the bundle will be removed instead of updated with the empty snapshot.

* **browser-toolbar**, **concept-toolbar**
  * ⚠️ **This is a breaking API change**: The interface of the "URL commit listener" changed from `(String) -> Unit` to `(String) -> Boolean`. If the function returns `true` then the toolbar will automatically switch to "display mode". If no function is set or if the function returns false the toolbar remains in "edit mode".
  * Added `private` field (`Boolean`): Enables/Disables private mode. In private mode the IME should not update any personalized data such as typing history and personalized language model based on what the user typed.
  * The background and foreground color of the autocomplete suggestion can now be styled:

  ```xml
  <mozilla.components.browser.toolbar.BrowserToolbar
      ...
      app:browserToolbarSuggestionBackgroundColor="#ffffcc00"
      app:browserToolbarSuggestionForegroundColor="#ffff4444"/>
  ```

# 0.42.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.41.0...v0.42.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/44?closed=1)
* [API reference](https://mozilla-mobile.github.io/android-components/api/0.42.0/index)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v0.42.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v0.42.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v0.42.0/buildSrc/src/main/java/Config.kt)

* **engine-gecko-nightly**
  * Now also serves as an implementation of `concept-fetch` by providing the new `GeckoViewFetchClient`. This allows applications to rely on Gecko's networking capabilities when issuing HTTP requests, even outside the browser view (GeckoView).
  
* **feature-prompts**, **browser-engine-gecko***
  * Added support for [JavaScript Confirm dialogs](https://developer.mozilla.org/en-US/docs/Web/API/Window/confirm).

* **feature-session**
  * Fixed an issue causing `EngineViewPresenter` to render a selected `Session` even though it was configured to show a fixed `Session`. This issue caused a crash (`IllegalStateException: Display already acquired`) in the [Reference Browser](https://github.com/mozilla-mobile/reference-browser) when a "Custom Tab" and the "Browser" tried to render the same `Session`.
  * Fixed an issue where back and forward button handling would not take place on the session whose ID was provided.

* **feature-search**
  * Added `SearchUseCases.NewTabSearchUseCase` and interface `SearchUseCase` (implemented by `DefaultSearchUseCase` and `NewTabSearchUseCase`).

* **browser-engine-system**
  * Added support for [JavaScript prompt alerts](https://developer.mozilla.org/en-US/docs/Web/API/Window/prompt) on WebView.

* **feature-customtabs**
  * Fixed an issue causing the `closeListener` to be invoked even when the current session isn't a Custom Tab.
  * Fixed an issue with the image resources in the toolbar were not tinted when an app provided a light colour for the background.

* **support-base**
  * Added `ViewBoundFeatureWrapper` for wrapping `LifecycleAwareFeature` references that will automatically be cleared if the provided `View` gets detached. This is helpful for fragments that want to keep a reference to a `LifecycleAwareFeature` (e.g. to be able call `onBackPressed()`) that itself has strong references to `View` objects. In cases where the fragment gets detached (e.g. to be added to the backstack) and the `View` gets detached (and destroyed) the wrapper will automatically stop the `LifecycleAwareFeature`  and clear all references..
  * Added generic `BackHandler` interface for fragments, features and other components that want to handle 'back' button presses.

* **ui-doorhanger**
  * 🆕 New component: A `Doorhanger` is a floating heads-up popup that can be anchored to a view. They are presented to notify the user of something that is important (e.g. a content permission request).

* **feature-sitepermissions**
  * 🆕 New component: A feature that will subscribe to the selected session, and will provide an UI for all the incoming appPermission and contentPermission request.

# 0.41.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.40.0...v0.41.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/43?closed=1)
* [API reference](https://mozilla-mobile.github.io/android-components/api/0.41.0/index)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v0.41.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v0.41.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v0.41.0/buildSrc/src/main/java/Config.kt)

* Mozilla App Services dependency upgraded: **0.15.0** 🔺
  * [0.15.0 release notes](https://github.com/mozilla/application-services/releases/tag/v0.15.0)

* **browser-engine-gecko-nightly**
  * Tweaked `NestedGeckoView` to "stick" to `AppBar` in nested scroll, like other Android apps. This is possible after a [fix](https://bugzilla.mozilla.org/show_bug.cgi?id=1515774) in APZ gesture detection.

* **feature-browser**
  * Added `BrowserToolbar` attributes to color the menu.

  ```xml
  <mozilla.components.browser.toolbar.BrowserToolbar
      android:id="@+id/toolbar"
      android:layout_width="match_parent"
      android:layout_height="56dp"
      android:background="#aaaaaa"
      app:browserToolbarMenuColor="@color/photonBlue50"
      app:browserToolbarInsecureColor="@color/photonRed50"
      app:browserToolbarSecureColor="@color/photonGreen50" />
  ```

* **feature-contextmenu**
  * Fixed Context Menus feature to work with Custom Tabs by passing in the session ID when applicable.

* **feature-customtabs**
  * Added a temporary workaround for Custom Tab intents not being recognized when using the Jetifier tool.

* **feature-downloads**
  * ⚠️ **This is a breaking API change!**
  * The required permissions are now passed to the `onNeedToRequestPermissions` callback.

  ```kotlin
  downloadsFeature = DownloadsFeature(
      requireContext(),
      sessionManager = components.sessionManager,
      fragmentManager = childFragmentManager,
      onNeedToRequestPermissions = { permissions ->
          requestPermissions(permissions, REQUEST_CODE_DOWNLOAD_PERMISSIONS)
      }
  )
  ```

  * Removed the `onPermissionsGranted` method in favour of `onPermissionsResult` which handles both granted and denied permissions. This method should be invoked from `onRequestPermissionsResult`:

  ```kotlin
   override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<String>, grantResults: IntArray) {
      when (requestCode) {
          REQUEST_CODE_DOWNLOAD_PERMISSIONS -> downloadsFeature.onPermissionsResult(permissions, grantResults)
      }
    }
  ```

  * Fixed Downloads feature to work with Custom Tabs by passing in the session ID when applicable.

 * **feature-prompts**
   * ⚠️ **This is a breaking API change!**
   * These change are similar to the ones for feature-downloads above and aim to provide a consistent way of handling permission requests.
   * The required permissions are now passed to the `onNeedToRequestPermissions` callback.

   ```kotlin
   promptFeature = PromptFeature(
      fragment = this,
      sessionManager = components.sessionManager,
      fragmentManager = requireFragmentManager(),
      onNeedToRequestPermissions = { permissions ->
          requestPermissions(permissions, REQUEST_CODE_PROMPT_PERMISSIONS)
      }
   )
   ```

   * Renamed `onRequestsPermissionsResult` to `onPermissionResult` and allow applications to specify the permission request code. This method should be invoked from `onRequestPermissionsResult`:

  ```kotlin
   override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<String>, grantResults: IntArray) {
      when (requestCode) {
          REQUEST_CODE_DOWNLOAD_PERMISSIONS -> downloadsFeature.onPermissionsResult(permissions, grantResults)
      }
    }
  ```

* **feature-contextmenu**
  * The component is now performing [haptic feedback](https://material.io/design/platform-guidance/android-haptics.html#) when showing a context menu.

* **browser-engine-gecko**, **browser-engine-gecko-beta**, **browser-engine-gecko-nightly**
  * After "Merge Day" and the release of Firefox 65 we updated our gecko-based components to follow the new upstream versions:
    * `browser-engine-gecko`: 65.0
    * `browser-engine-gecko-beta`: 66.0
    * `browser-engine-gecko-nightly`: 67.0

* **browser-toolbar**
  * Toolbar URL autocompletion is now performed off the UI thread.

* **concept-storage**
  * ⚠️ **This is a breaking API change!**
  * `HistoryAutocompleteResult` now includes an `input` field.

* **browser-domains**
  * ⚠️ **This is a breaking API change!**
  * `DomainAutocompleteResult` now includes an `input` field.

# 0.40.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.39.0...v0.40.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/42?closed=1)
* [API reference](https://mozilla-mobile.github.io/android-components/api/0.40.0/index)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v0.40.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v0.40.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v0.40.0/buildSrc/src/main/java/Config.kt)

* **support-ktx**
  * Added `Lifecycle.addObservers` to observe the lifecycle for multiple classes.

  ```kotlin
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
      lifecycle.addObservers(
        fullscreenFeature,
        sessionFeature,
        customTabsToolbarFeature
      )
    }
    ```

* **feature-awesomebar**
  * Added ability to show one item per search suggestion ([#1779](https://github.com/mozilla-mobile/android-components/issues/1779))
  * Added ability to define custom hooks to be invoked when editing starts or is completed.

* **browser-awesomebar**
  * Added ability to let consumers define the layouting of suggestions by implementing `SuggestionLayout` in order to control layout inflation and view binding.

  ```kotlin
  // Create a ViewHolder for your custom layout.
  class CustomViewHolder(view: View) : SuggestionViewHolder(view) {
      private val textView = view.findViewById<TextView>(R.id.text)

      override fun bind(
          suggestion: AwesomeBar.Suggestion,
          selectionListener: () -> Unit
      ) {
          textView.text = suggestion.title
          textView.setOnClickListener {
              suggestion.onSuggestionClicked?.invoke()
              selectionListener.invoke()
          }
      }
  }

  // Create a custom SuggestionLayout for controling view inflation
  class CustomSuggestionLayout : SuggestionLayout {
      override fun getLayoutResource(suggestion: AwesomeBar.Suggestion): Int {
          return android.R.layout.simple_list_item_1
      }

      override fun createViewHolder(awesomeBar: BrowserAwesomeBar, view: View, layoutId: Int): SuggestionViewHolder {
          return CustomViewHolder(view)
      }
  }
  ```

  * Added ability to transform suggestions returned by provider (adding data, removing data, filtering suggestions, ...)

  ```kotlin
  awesomeBar.transformer = object : SuggestionTransformer {
      override fun transform(
          provider: AwesomeBar.SuggestionProvider,
          suggestions: List<AwesomeBar.Suggestion>
      ): List<AwesomeBar.Suggestion> {
          return suggestions.map { suggestion ->
              suggestion.copy(title = "Awesome!")
          }
      }
  }

  // Use the custom layout with a BrowserAwesomeBar instance
  awesomeBar.layout = CustomSuggestionLayout()
  ```

* **lib-publicsuffixlist**
  * The public suffix list shipping with this component is now updated automatically in the repository every day (if there are changes).
  * Fixed an issue when comparing domain labels against the public suffix list ([#1777](https://github.com/mozilla-mobile/android-components/issues/1777))

* **feature-prompts**, **browser-engine-gecko***
  * Added support for [Pop-up windows dialog](https://support.mozilla.org/en-US/kb/pop-blocker-settings-exceptions-troubleshooting#w_what-are-pop-ups).

* **browser-engine-system**
  * Preventing JavaScript `confirm()` and `prompt()` until providing proper implementation #1816.

* **feature-search**, **feature-session**
  * `SessionUseCases` and `SearchUseCases` now take an optional `onNoSession: String -> Session` lambda parameter. This function will be invoked when executing a use case that requires a (selected) `Session` and no such session is available. This makes using the use cases and feature components useable in browsers that may not always have sessions. The default implementation creates a new `Session` and adds it to the `SessionManager`.

* **support-rustlog**
  * 🆕 New component: This component allows consumers of [megazorded](https://mozilla.github.io/application-services/docs/applications/consuming-megazord-libraries.html) Rust libraries produced by application-services to redirect their log output to the base component's log system as follows:
  ```kotlin
  import mozilla.components.support.rustlog.RustLog
  import mozilla.components.support.base.log.Log
  // In onCreate, any time after MyMegazordClass.init()
  RustLog.enable()
  // Note: By default this is enabled at level DEBUG, which can be adjusted.
  // (It is recommended you do this for performance if you adjust
  // `Log.logLevel`).
  RustLog.setMaxLevel(Log.Priority.INFO)
  // You can also enable "trace logs", which may include PII
  // (but can assist debugging) as follows. It is recommended
  // you not do this in builds you distribute to users.
  RustLog.setMaxLevel(Log.Priority.DEBUG, true)
  ```
  * This is pointless to do when not using a megazord.
    * Megazording is required due to each dynamically loaded Rust library having its own internal/private version of the Rust logging framework. When megazording, this is still true, but there's only a single dynamically loaded library, and so it's redirected properly. (This could probably be worked around, but it would take a great effort, and given that we expect most production use of these libraries will be using megazords, we accepted this limitation)
    * This would be very annoying during initial development (and debugging the sample apps), so by default, we'll log (directly, e.g. not through the base component logger) to logcat when not megazorded.
  * Note that you must call `MyMegazordClass.init()` *before* any uses of this class.

* Mozilla App Services library updated to 0.14.0. See [release notes](https://github.com/mozilla/application-services/releases/tag/v0.14.0) for details.
  * Important: Users consuming megazords must also update the application-services gradle plugin to version 0.3.0.

* **feature-findinpage**
  * 🆕 A new feature component for [finding text in a web page](https://support.mozilla.org/en-US/kb/search-contents-current-page-text-or-links). [Documentation](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/findinpage/README.md).

* **service-firefox-accounts**
  * Added `FxaAccountManager`, which encapsulates a lower level accounts API and provides an observable interface for consumers that wish to be notified of account and profile changes.
  * Background-worker friendly.
  ```kotlin
  // Long-lived instance, pinned on an application.
  val accountManager = FxaAccountManager(context, Config.release(CLIENT_ID, REDIRECT_URL), arrayOf("profile"))
  launch { accountManager.init() }

  // Somewhere in a fragment that cares about account state...
  accountManager.register(object : AccountObserver {
      override fun onLoggedOut() {
        ...
      }

      override fun onAuthenticated(account: FirefoxAccountShaped) {
        ...
      }

      override fun onProfileUpdated(profile: Profile) {
        ...
      }

      override fun onError(error: FxaException) {
        ...
      }
  }

  // Reacting to a "sign-in" user action:
  launch {
    val authUrl = try {
        accountManager.beginAuthentication().await()
    } catch (error: FxaException) {
        // ... display error ui...
        return@launch
    }
    openWebView(authUrl)
  }

  ```

* **feature-accounts** 🆕
  * Added a new `FirefoxAccountsAuthFeature`, which ties together the **FxaAccountManager** with a session manager via **feature-tabs**.

* **browser-toolbar**
  * Fixing bug that allowed text behind the security icon being selectable. [Issue #448](https://github.com/mozilla-mobile/reference-browser/issues/448)

# 0.39.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.38.0...v0.39.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/41?closed=1)
* [API reference](https://mozilla-mobile.github.io/android-components/api/0.39.0/index)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/master/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/master/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/master/buildSrc/src/main/java/Config.kt)

* **feature-awesomebar**
  * Added `ClipboardSuggestionProvider` - An `AwesomeBar.SuggestionProvider` implementation that returns a suggestions for an URL in the clipboard (if there's any).

* **feature-prompts**, **browser-engine-gecko**
  * Added support for [Window.prompt](https://developer.mozilla.org/en-US/docs/Web/API/Window/prompt).
  * Fixing Issue [#1771](https://github.com/mozilla-mobile/android-components/issues/1771). Supporting single choice items with sub-menus group.

* **browser-engine-gecko-nightly**
  * The GeckoView Nightly dependency is now updated to the latest version automatically in cases where no code changes are required.

* **browser-menu**
  * Added [docs](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/menu/README.md#browsermenu) for customizing `BrowserMenu`.
  * Added `BrowserMenuDivider`. [For customization take a look at the docs.](https://github.com/mozilla-mobile/android-components/tree/master/components/browser/menu/README.md#BrowserMenuDivider)
  * Added [BrowserMenuImageText](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/menu/README.md#BrowserMenuImageText) for show an icon next to text in menus.
  * Added support for showing a menu with DOWN and UP orientation (e.g. for supporting menus in bottom toolbars).

* **concept-engine**, **browser-engine-gecko-***
  * Added support for enabling tracking protection for specific session type:
  ```kotlin
  val engine = GeckoEngine(runtime, DefaultSettings(
    trackingProtectionPolicy = TrackingProtectionPolicy.all().forPrivateSessionsOnly())
  )
  ```

* **browser-toolbar**
  * Added `BrowserToolbarBottomBehavior` - a [CoordinatorLayout.Behavior](https://developer.android.com/reference/android/support/design/widget/CoordinatorLayout.Behavior) implementation to be used when placing `BrowserToolbar` at the bottom of the screen. This behavior will:
    * Show/Hide the `BrowserToolbar` automatically when scrolling vertically.
    * On showing a [Snackbar] position it above the `BrowserToolbar`.
    * Snap the `BrowserToolbar` to be hidden or visible when the user stops scrolling.

* **lib-publicsuffixlist**
  * 🆕 A new component/library for reading and using the [public suffix list](https://publicsuffix.org/). Details can be found in our [docs](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/publicsuffixlist/README.md).

# 0.38.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.37.0...v0.38.0),
[Milestone](https://github.com/mozilla-mobile/android-components/milestone/40?closed=1),
[API reference](https://mozilla-mobile.github.io/android-components/api/0.38.0/index)

* Compiled against:
  * Android (SDK: 28, Support Libraries: 28.0.0)
  * Kotlin (Stdlib: 1.3.10, Coroutines: 1.0.1)
  * GeckoView (Nightly: **66.0.20190111093148** 🔺, Beta: 65.0.20181211223337, Release: 64.0.20181214004633)
  * Mozilla App Services (FxA: **0.13.3** 🔺, Sync Logins: **0.13.3** 🔺, Places: **0.13.3** 🔺)
    * [0.13.0 release notes](https://github.com/mozilla/application-services/releases/tag/v0.13.0)
    * [0.13.1 release notes](https://github.com/mozilla/application-services/releases/tag/v0.13.1)
    * [0.13.2 release notes](https://github.com/mozilla/application-services/releases/tag/v0.13.2)
    * [0.13.3 release notes](https://github.com/mozilla/application-services/releases/tag/v0.13.3)
  * Third Party Libs (Sentry: 1.7.14, Okhttp: 3.12.0)

* **support-utils**
  * [Improve URL toolbar autocompletion matching](https://github.com/mozilla-mobile/android-components/commit/ff25ec3e6646736e2b4ba3ee1d9fdd9a8412ce8c).

* **browser-session**
  * [Improving tab selection algorithm, when removing the selected tab.](https://github.com/mozilla-mobile/android-components/issues/1518)
  * [Saving the state when the app goes to the background no longer blocks the UI thread.](https://github.com/mozilla-mobile/android-components/commit/ea811e089cd40c6d1fc9ec688fa5db3e7b023331)

* **browser-engine-system**
  * Added support for JavaScript alerts on SystemEngineView.
  * [Improving use of internal Webview](https://github.com/mozilla-mobile/android-components/commit/59240f7a71a9f63fc51c1ff65e604f6735196a0e).

* **feature-customtabs**
  * Added a close button to a custom tab with back button handling.

* **feature-prompts**, **browser-engine-gecko**
  * Added support for Authentication dialogs.
  * Added support for [datetime-local](https://developer.mozilla.org/en-US/docs/Web/HTML/Element/input/datetime-local) and [time](https://developer.mozilla.org/en-US/docs/Web/HTML/Element/input/time) pickers.
  * Added support for [input type color fields](https://developer.mozilla.org/en-US/docs/Web/HTML/Element/input/color).

* **browser-menu**
  * `BrowserMenuItemToolbar` now allows overriding the `visible` lambda.

* **service-sync-logins**, **service-firefox-accounts**, **concept-storage**
  * Updated underlying library from 0.12.1 to 0.13.3, see the [release notes for 0.13.0](https://github.com/mozilla/application-services/blob/master/CHANGELOG.md#0130-2019-01-09) for futher details on the most substantial changes. ([#1690](https://github.com/mozilla-mobile/android-components/issues/1690))
    * sync-logins: Added a new `wipeLocal` method, for clearing all local data.
    * sync-logins: Removed `reset` because it served a nonexistant use case, callers almost certainly want `wipeLocal` or `wipe` instead.
    * sync-logins: Added `ensureLocked` and `ensureUnlocked` for cases where checking `isLocked` is inconvenient or requires additional locking.
    * sync-logins: Allow storage to be unlocked using a `ByteArray` instead of a `String`.
    * firefox-accounts: Network errors will now be reported as instances of FxaException.Network, instead of `FxaException.Unspecified`.
    * history (concept-storage): PII is no longer logged during syncing (or any other time).

# 0.37.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.36.0...v0.37.0),
[Milestone](https://github.com/mozilla-mobile/android-components/milestone/39?closed=1),
[API reference](https://mozilla-mobile.github.io/android-components/api/0.37.0/index)

* Compiled against:
  * Android (SDK: 28, Support Libraries: 28.0.0)
  * Kotlin (Stdlib: 1.3.10, Coroutines: 1.0.1)
  * GeckoView (Nightly: 66.0.20181217093726, Beta: 65.0.20181211223337, Release: 64.0.20181214004633)
  * Mozilla App Services (FxA: 0.12.1, Sync Logins: 0.12.1, Places: 0.12.1)
  * Third Party Libs (Sentry: 1.7.14, Okhttp: 3.12.0)

* **feature-customtabs**
  * Added a new feature `CustomTabsToolbarFeature` which will handle setting up the `BrowserToolbar` with the configurations available in that session:

  ```kotlin
  CustomTabsToolbarFeature(
    sessionManager,
    browserToolbar,
    sessionId
  ).also { lifecycle.addObserver(it) }
  ```
  Note: this constructor API is still a work-in-progress and will change as more Custom Tabs support is added to it next release.

  * Fixed a bug where a third-party app (like Gmail or Slack) could crash when calling warmup().

* **feature-session-bundling**
  * 🆕 New component that saves the state of sessions (`SessionManager.Snapshot`) in grouped bundles (e.g. by time).

* **service-telemetry**
  * Added new "pocket event" ping builder ([#1606](https://github.com/mozilla-mobile/android-components/issues/1606))
  * Added ability to get ping builder by type from `Telemetry` instance.
  * ⚠️ **This is a breaking change!** <br/>
  HttpURLConnectionTelemetryClient was removed. *service-telemetry* is now using [*concept-fetch*](https://github.com/mozilla-mobile/android-components/tree/master/components/concept/fetch) which allows consumers to use a unified http client. There are two options available currently: [lib-fetch-httpurlconnection](https://github.com/mozilla-mobile/android-components/tree/master/components/lib/fetch-httpurlconnection) (Based on [HttpURLConnection](https://developer.android.com/reference/java/net/HttpURLConnection)) and [lib-fetch-okhttp](https://github.com/mozilla-mobile/android-components/tree/master/components/lib/fetch-okhttp) (Based on [OkHttp](https://github.com/square/okhttp)).

  ```Kotlin
  // Using HttpURLConnection:
  val client = new TelemetryClient(HttpURLConnectionClient())

  // Using OkHttp:
  val client = TelemetryClient(OkHttpClient())

  val telemetry = Telemetry(configuration, storage, client, scheduler)
  ```

* **browser-search**
  * Updated search plugins ([#1563](https://github.com/mozilla-mobile/android-components/issues/1563))

* **ui-autocomplete**
  * Fixed a bug where pressing backspace could skip a character ([#1489](https://github.com/mozilla-mobile/android-components/issues/1489)).

* **feature-customtabs**
  * Fixed a bug where a third-party app (like Gmail or Slack) could crash when calling warmup().

* **browser-session**
  * Added ability to notify observers when desktop mode changes (`onDesktopModeChange`)

* **browser-menu**
  * Added new `BrowserMenuSwitch` for using switch widgets inside the menu.

* **support-ktx**
  * Added extension method `Bitmap.withRoundedCorners(cornerRadiusPx: Float)`

* **support-base**
  * Introduced `LifecycleAwareFeature` for writing features that depend on a lifecycle.


# 0.36.1

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.36.0...v0.36.1)

* Compiled against:
  * Android (SDK: 28, Support Libraries: 28.0.0)
  * Kotlin (Stdlib: 1.3.10, Coroutines: 1.0.1)
  * GeckoView (Nightly: 66.0.20181217093726, Beta: 65.0.20181211223337, Release: 64.0.20181214004633)
  * Mozilla App Services (FxA: 0.12.1, Sync Logins: 0.12.1, Places: 0.12.1)
  * Third Party Libs (Sentry: 1.7.14, Okhttp: 3.12.0)

* **feature-customtabs**
  * Fixed a bug where a third-party app (like Gmail or Slack) could crash when calling warmup().

# 0.36.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.35.0...v0.36.0),
[Milestone](https://github.com/mozilla-mobile/android-components/milestone/38?closed=1),
[API reference](https://mozilla-mobile.github.io/android-components/api/0.36.0/index)

* Compiled against:
  * Android (SDK: 28, Support Libraries: 28.0.0)
  * Kotlin (Stdlib: 1.3.10, Coroutines: 1.0.1)
  * GeckoView (Nightly: 66.0.20181217093726, Beta: 65.0.20181211223337, Release: 64.0.20181214004633)
  * Mozilla App Services (FxA: 0.12.1, Sync Logins: 0.12.1, Places: 0.12.1)
  * Third Party Libs (Sentry: 1.7.14, Okhttp: 3.12.0)

* **browser-session**
  * Added a use case for exiting fullscreen mode.

  ```kotlin
  val sessionUseCases = SessionUseCases(sessionManager)
  if (isFullScreenMode) {
    sessionUseCases.exitFullscreen.invoke()
  }
  ```

  * We also added a `FullScreenFeature` that manages fullscreen support.

  ```kotlin
  val fullScreenFeature = FullScreenFeature(sessionManaager, sessionUseCases) { enabled ->
    if (enabled) {
      // Make custom views hide.
    } else {
      // Make custom views unhide.
    }
  }

  override fun onBackPressed() : Boolean {
    // Handling back presses when in fullscreen mode
    return fullScreenFeature.onBackPressed()
  }
  ```
* **feature-customtabs**
  * Added support for opening speculative connections for a likely future navigation to a URL (`mayLaunchUrl`)

* **feature-prompts**, **engine-gecko-***, **engine-system**
  * Added support for file picker requests.

    There some requests that are not handled with dialogs, instead they are delegated to other apps
    to perform the request, an example is a file picker request. As a result, now you have to override
    `onActivityResult` on your `Activity` or `Fragment` and forward its calls to `promptFeature.onActivityResult`.

    Additionally, there are requests that need some permission to be granted before they can be performed, like
    file pickers that need access to read the selected files. Like `onActivityResult` you need to override
    `onRequestPermissionsResult` and forward its calls to `promptFeature.onRequestPermissionsResult`.

* **browser-toolbar**
  * The "urlBoxView" is now drawn behind the site security icon (in addition to the URL and the page actions)

# 0.35.1

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.35.0...v0.35.1)

* Compiled against:
  * Android (SDK: 28, Support Libraries: 28.0.0)
  * Kotlin (Stdlib: 1.3.10, Coroutines: 1.0.1)
  * GeckoView (Nightly: 66.0.20181217093726, Beta: 65.0.20181211223337, Release: 64.0.20181214004633)
  * Mozilla App Services (FxA: **0.12.1** 🔺, Sync Logins: **0.12.1** 🔺, Places: **0.12.1** 🔺)
  * Third Party Libs (Sentry: 1.7.14, Okhttp: 3.12.0)

* Re-release of 0.34.1 with updated App Services dependencies (0.12.1).

# 0.35.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.34.0...v0.35.0),
[Milestone](https://github.com/mozilla-mobile/android-components/milestone/37?closed=1),
[API reference](https://mozilla-mobile.github.io/android-components/api/0.35.0/index)

* Compiled against:
  * Android (SDK: 28, Support Libraries: 28.0.0)
  * Kotlin (Stdlib: 1.3.10, Coroutines: 1.0.1)
  * GeckoView (Nightly: **66.0.20181217093726** 🔺, Beta: **65.0.20181211223337** 🔺, Release: **64.0.20181214004633** 🔺)
  * Mozilla App Services (FxA: 0.11.5, Sync Logins: 0.11.5, Places: 0.11.5)
  * Third Party Libs (Sentry: 1.7.14, Okhttp: 3.12.0)

* **browser-errorpages**
  * Localized strings for de, es, fr, it, ja, ko, zh-rCN, zh-rTW.

* **feature-customtabs**
  * Added support for warming up the browser process asynchronously.
  * ⚠️ **This is a breaking change**
  * `CustomTabsService` has been renamed to `AbstractCustomTabsService` and is now an abstract class in order to allow apps to inject the `Engine` they are using. An app that wants to support custom tabs will need to create its own class and reference it in the manifest:

  ```kotlin
  class CustomTabsService : AbstractCustomTabsService() {
    override val engine: Engine by lazy { components.engine }
  }
  ```
* **feature-prompts**
  * Added support for alerts dialogs.
  * Added support for date picker dialogs.

* **feature-tabs**
  * Added support to remove all or specific types of tabs to the `TabsUseCases`.

  ```kotlin
  // Remove all tabs
  tabsUseCases.removeAllTabs.invoke()
  // Remove all regular tabs
  tabsUseCases.removeAllTabsOfType.invoke(private = false)
  // Remove all private tabs
  tabsUseCases.removeAllTabsOfType.invoke(private = true)
  ```

* **support-ktx**
  New extension function `toDate` that converts a string to a Date object from a formatter input.
  ```kotlin
       val date = "2019-11-28".toDate("yyyy-MM-dd")
  ```

* **concept-engine**, **engine-gecko-beta**, **engine-gecko-nightly**:
  * Add setting to enable testing mode which is used in engine-gecko to set `FULL_ACCESSIBILITY_TREE` to `true`. This allows access to the full DOM tree for testing purposes.

  ```kotlin
  // Turn testing mode on by default when the engine is created
  val engine = GeckoEngine(runtime, DefaultSettings(testingModeEnabled=true))

  // Or turn testing mode on at a later point
  engine.settings.testingModeEnabled = true
  ```

  * The existing `userAgentString` setting is now supported by `engine-gecko-beta` and `engine-gecko-nightly`.

* **feature-session**
  * Added a `HistoryTrackingDelegate` implementation, which previously lived in **feature-storage**.

* **feature-storage**
  * Removed! See **feature-session** instead.

* **sample-browser**
  * Added in-memory browsing history as one of the AwesomeBar data providers.

* **feature-sync**
  * Simplified error handling. Errors are wrapped in a SyncResult, exceptions are no longer thrown.
  * `FirefoxSyncFeature`'s constructor now takes a map of `Syncable` instances. That is, the internal list of `Syncables` is no longer mutable.
  * `sync` is now a `suspend` function. Callers are expected to manage scoping themselves.
  * Ability to observe "sync is running" and "sync is idle" events vs `SyncStatusObserver` interface.
  * Ability to query for current sync state (running or idle).
  * See included `sample-sync-history` application for example usage of these observers.

# 0.34.2

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.34.1...v0.34.2)

* Compiled against:
  * Android (SDK: 28, Support Libraries: 28.0.0)
  * Kotlin (Stdlib: 1.3.10, Coroutines: 1.0.1)
  * GeckoView (Nightly: 65.0.20181129095546, Beta: 64.0.20181022150107, Release: 63.0.20181018182531)
  * Mozilla App Services (FxA: **0.11.5** 🔺, Sync Logins: **0.11.5** 🔺, Places: **0.11.5** 🔺)
  * Third Party Libs (Sentry: 1.7.14, Okhttp: 3.12.0)

* Re-release of 0.34.1 with updated App Services dependencies (0.11.5).

# 0.34.1

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.34.0...v0.34.1)

* Compiled against:
  * Android (SDK: 28, Support Libraries: 28.0.0)
  * Kotlin (Stdlib: 1.3.10, Coroutines: 1.0.1)
  * GeckoView (Nightly: **65.0.20181129095546** 🔺, Beta: 64.0.20181022150107, Release: 63.0.20181018182531)
  * Mozilla App Services (FxA: 0.11.2, Sync Logins: 0.11.2, Places: 0.11.2)
  * Third Party Libs (Sentry: 1.7.14, Okhttp: 3.12.0)

* **browser-engine-gecko-nightly**
  * Updated GeckoView dependency.

# 0.34.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.33.0...v0.34.0),
[Milestone](https://github.com/mozilla-mobile/android-components/milestone/36?closed=1),
[API reference](https://mozilla-mobile.github.io/android-components/api/0.34.0/index)

* Compiled against:
  * Android (SDK: 28, Support Libraries: 28.0.0)
  * Kotlin (Stdlib: 1.3.10, Coroutines: 1.0.1)
  * GeckoView (Nightly: 65.0.20181123100059, Beta: 64.0.20181022150107, Release: 63.0.20181018182531)
  * Mozilla App Services (FxA: **0.11.2** 🔺, Sync Logins: **0.11.2** 🔺, Places: **0.11.2** 🔺)
  * Third Party Libs (Sentry: 1.7.14, Okhttp: 3.12.0)

* **browser-engine-gecko-nightly**
  * Added support for observing history events and providing visited URLs (`HistoryTrackingDelegate`).

* **browser-engine-system**
  * Fixed a crash when calling `SystemEngineSession.saveState()` from a non-UI thread.

* **browser-awesomebar**
  * Added ability for search suggestions to span multiple rows.

* **browser-toolbar**
  * Fixed rendering issue when displaying site security icons.

* **feature-prompts**
  * 🆕 New component: A component that will subscribe to the selected session and will handle all the common prompt dialogs from web content.

  ```kotlin
  val promptFeature = PromptFeature(sessionManager,fragmentManager)

  //It will start listing for new prompt requests for web content.
  promptFeature.start()

  //It will stop listing for future prompt requests for web content.
  promptFeature.stop()
  ```

* **feature-session**, **browser-session**, **concept-engine**, **browser-engine-system**:
  * Added functionality to observe window requests from the browser engine. These requests can be observed on the session directly using `onOpenWindowRequest` and `onCloseWindowRequest`, but we also provide a feature class, which will automatically open and close the corresponding window:

  ```Kotlin
  windowFeature = WindowFeature(engine, sessionManager)

  override fun onStart() {
    windowFeature.start()
  }

  override fun onStop() {
    windowFeature.stop()
  }

  ```

  In addition, to observe window requests the new engine setting `supportMultipleWindows` has to be set to true:

  ```Kotlin
  val engine = SystemEngine(context,
    DefaultSettings(
      supportMultipleWindows = true
    )
  )
  ```

* **concept-storage**, **browser-storage-sync**, **services-logins-sync**:
  * Added a new interface, `SyncableStore<AuthType>`, which allows a storage layer to be used with `feature-sync`.
  * Added a `SyncableStore<SyncAuthInfo>` implementation for `browser-storage-sync`
  * Added a `SyncableStore<SyncUnlockInfo>` implementation for `services-logins-sync`.

* **feature-sync**:
  * 🆕 New component: A component which orchestrates synchronization of groups of similar `SyncableStore` objects using a `FirefoxAccount`.
  * Here is an example of configuring and synchronizing a places-backed `HistoryStorage` (provided by `browser-storage-sync` component):

  ```Kotlin
  val historyStorage = PlacesHistoryStorage(context)
  val featureSync = FirefoxSyncFeature(Dispatchers.IO + job) { authInfo ->
      SyncAuthInfo(
          fxaAccessToken = authInfo.fxaAccessToken,
          kid = authInfo.kid,
          syncKey = authInfo.syncKey,
          tokenserverURL = authInfo.tokenServerUrl
      )
  }.also {
      it.addSyncable("placesHistory", historyStorage)
  }
  val syncResult = featureSync.sync().await()
  assert(syncResults["placesHistory"]!!.status is SyncOk)
  ```

* **service-firefox-accounts**:
  * ⚠️ **This is a breaking change**
  * We've simplified the API to provide the FxA configuration:

  ```Kotlin
  // Before
  Config.custom(CONFIG_URL).await().use {
    config -> FirefoxAccount(config, CLIENT_ID, REDIRECT_URL)
  }

  // Now
  val config = Config(CONFIG_URL, CLIENT_ID, REDIRECT_URL)
  FirefoxAccount(config)
  ```

  A full working example can be found [here](https://github.com/mozilla-mobile/android-components/blob/master/samples/firefox-accounts/src/main/java/org/mozilla/samples/fxa/MainActivity.kt).

# 0.33.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.32.0...v0.33.0),
[Milestone](https://github.com/mozilla-mobile/android-components/milestone/35?closed=1),
[API reference](https://mozilla-mobile.github.io/android-components/api/0.33.0/index)

* Compiled against:
  * Android (SDK: 28, Support Libraries: 28.0.0)
  * Kotlin (Stdlib: **1.3.10** 🔺, Coroutines: 1.0.1)
  * GeckoView (Nightly: **65.0.20181123100059** 🔺, Beta: 64.0.20181022150107, Release: 63.0.20181018182531)
  * Mozilla App Services (FxA: 0.10.0, Sync Logins: 0.10.0, Places: 0.10.0)
  * Third Party Libs (Sentry: 1.7.14, Okhttp: 3.12.0)

* **feature-contextmenu**
  * 🆕 New component: A component for displaying context menus when *long-pressing* web content.

* **concept-toolbar**: 🆕 Added autocomplete support
  * Toolbar concept got a new `setAutocompleteListener` method.
  * Added `AutocompleteDelegate` concept which allows tying together autocomplete results with a UI presenter.

* **concept-storage** and all of its implementations
  * ⚠️ **This is a breaking change**
  * Renamed `getDomainSuggestion` to `getAutocompleteSuggestion`, which now returns a `HistoryAutocompleteResult`.

* **feature-toolbar**
  * 🆕 Added new `ToolbarAutocompleteFeature`:

  ```Kotlin
  toolbarAutocompleteFeature = ToolbarAutocompleteFeature(toolbar).apply {
    this.addHistoryStorageProvider(components.historyStorage)
    this.addDomainProvider(components.shippedDomainsProvider)
  }
  ```

* **samples-browser**, **samples-toolbar**
  * Converted these samples to use the new `ToolbarAutocompleteFeature`.

* **feature-session**
  * Introducing `CoordinateScrollingFeature` a new feature to coordinate scrolling behavior between an `EngineView` and the view that you specify. For a full example take a look at its usages in [Sample Browser](https://github.com/mozilla-mobile/android-components/tree/master/samples/browser).

* **feature-tabs**
  * Added a filter to `TabsFeature` to allow you to choose which sessions to show in the TabsTray. This is particularly useful if you want to filter out private tabs based on some UI interaction:

  ```kotlin
  val tabsFeature = TabsFeature(
    tabsTray,
    sessionManager,
    closeTabsTray = closeTabs()
  )
  tabsFeature.filterTabs {
    it.private
  }
  ```

* **engine-gecko,engine-gecko-beta and engine-gecko-nightly**
  * Fixing bug [#1333](https://github.com/mozilla-mobile/android-components/issues/1333). This issue didn't allow to use a `GeckoEngineSession` after sending a crash report.

# 0.32.2

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.32.1...v0.32.2),
[API reference](https://mozilla-mobile.github.io/android-components/api/0.32.0/index)

* Compiled against:
  * Android (SDK: **28** 🔺, Support Libraries: **28.0.0** 🔺)
  * Kotlin (Stdlib: 1.3.0, Coroutines: 1.0.1)
  * GeckoView (Nightly: **65.0.20181116100120** 🔺, Beta: 64.0.20181022150107, Release: 63.0.20181018182531)
  * Mozilla App Services (FxA: **0.10.0** 🔺, Sync Logins: **0.10.0** 🔺, Places: **0.10.0** 🔺)

* **ui-autocomplete**
  * Fixed problem handling backspaces as described in [Issue 1489](https://github.com/mozilla-mobile/android-components/issues/1489)

* **browser-search**
  * Updated search codes (see [Issue 1563](https://github.com/mozilla-mobile/android-components/issues/1563) for details)

# 0.32.1

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.32.0...v0.32.1),
[API reference](https://mozilla-mobile.github.io/android-components/api/0.32.0/index)

* Compiled against:
  * Android (SDK: **28** 🔺, Support Libraries: **28.0.0** 🔺)
  * Kotlin (Stdlib: 1.3.0, Coroutines: 1.0.1)
  * GeckoView (Nightly: **65.0.20181116100120** 🔺, Beta: 64.0.20181022150107, Release: 63.0.20181018182531)
  * Mozilla App Services (FxA: **0.10.0** 🔺, Sync Logins: **0.10.0** 🔺, Places: **0.10.0** 🔺)

* **browser-session**
  * Fixed concurrency problem and related crash described in [Issue 1624](https://github.com/mozilla-mobile/android-components/issues/1624)

# 0.32.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.31.0...v0.32.0),
[Milestone](https://github.com/mozilla-mobile/android-components/milestone/34?closed=1),
[API reference](https://mozilla-mobile.github.io/android-components/api/0.32.0/index)

* Compiled against:
  * Android (SDK: **28** 🔺, Support Libraries: **28.0.0** 🔺)
  * Kotlin (Stdlib: 1.3.0, Coroutines: 1.0.1)
  * GeckoView (Nightly: **65.0.20181116100120** 🔺, Beta: 64.0.20181022150107, Release: 63.0.20181018182531)
  * Mozilla App Services (FxA: **0.10.0** 🔺, Sync Logins: **0.10.0** 🔺, Places: **0.10.0** 🔺)

* ⚠️ **This is the first release compiled against Android SDK 28.**

* **browser-domains**
  * Deprecated `DomainAutoCompleteProvider` in favour of `CustomDomainsProvider` and `ShippedDomainsProvider`.

* **lib-crash**
  * The state of the "Send crash report" checkbox is now getting saved and restored once the dialog is shown again.
  * The crash reporter can now sends reports if the prompt is closed by pressing the back button.

* **lib-fetch-httpurlconnection**
  * 🆕 New component: `concept-fetch` implementation using [HttpURLConnection](https://developer.android.com/reference/java/net/HttpURLConnection.html).

* **lib-fetch-okhttp**
  * 🆕 New component: `concept-fetch` implementation using [OkHttp](https://github.com/square/okhttp).

* **browser-session**:
  * Replace `DefaultSessionStorage` with a new configurable implementation called `SessionStorage`:

  ```kotlin
  SessionStorage().autoSave(sessionManager)
    .periodicallyInForeground(interval = 30, unit = TimeUnit.SECONDS)
    .whenGoingToBackground()
    .whenSessionsChange()
  ```

# 0.31.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.30.0...v0.31.0),
[Milestone](https://github.com/mozilla-mobile/android-components/milestone/33?closed=1),
[API reference](https://mozilla-mobile.github.io/android-components/api/0.31.0/index)

* Compiled against:
  * Android (SDK: 27, Support Libraries: 27.1.1)
  * Kotlin (Stdlib: **1.3.0** 🔺, Coroutines: **1.0.1** 🔺)
  * GeckoView (Nightly: **65.0.20181107100135** 🔺, Beta: 64.0.20181022150107, Release: 63.0.20181018182531)

* **concept-storage**, **browser-storage-memory**, **browser-storage-sync**
  * Added a `getDomainSuggestion` method to `HistoryStorage` which is intended to power awesomebar-like functionality.
  * Added basic implementations of `getDomainSuggestion` to existing storage components.

* **browser-session**, **concept-engine**, **browser-engine-system**, **browser-engine-gecko(-beta/nightly)**:
  * ⚠️ **This is a breaking change**
  * Added functionality to observe permission requests from the browser engine. We have now removed the workaround that automatically granted permission to play protected (DRM) media. Permission requests can be observed via the browser session:

  ```Kotlin
  // Grant permission to all DRM content
  permissionObserver = object : SelectionAwareSessionObserver(sessionManager) {
      override fun onContentPermissionRequested(session: Session, permissionRequest: PermissionRequest): Boolean =
          permissionRequest.grantIf { it is Permission.ContentProtectedMediaId }
  }

  override fun onStart() {
    // Observe permission requests on selected sessions
    permissionObserver.observeSelected()
  }

  override fun onStop() {
    permissionObserver.stop()
  }

  ```

* **concept-engine**, **engine-system**:
  * ⚠️ **This is a breaking change**
  * Web font blocking is now controlled by an engine setting only. `TrackingProtectionPolicy.WEBFONTS` was removed:

  ```Kotlin
  // Disable web fonts by default
  SystemEngine(runtime, DefaultSettings(webFontsEnabled = false))
  ```

* **feature-customtabs**
  * 🆕 New component for providing custom tabs functionality. `CustomTabsService` was moved from `browser-session` to this new component.

* **browser-awesomebar**
  * Various colors of the Awesome Bar can now be styled:

  ```XML
    <mozilla.components.browser.awesomebar.BrowserAwesomeBar
      ..
      mozac:awesomeBarTitleTextColor="#ffffff"
      mozac:awesomeBarDescriptionTextColor="#dddddd"
      mozac:awesomeBarChipTextColor="#ffffff"
      mozac:awesomeBarChipBackgroundColor="#444444" />
  ```

* **browser-toolbar**, **feature-toolbar**
  * Added support for displaying the site security indicator (lock/globe icon).

* **concept-fetch**
  * 🆕 New component defining an abstract definition of an HTTP client for fetching resources. Later releases will come with components implementing this concept using HttpURLConnection, OkHttp and Necko/GeckoView. Eventually all HTTP client code in the components will be replaced with `concept-fetch` and consumers can decide what HTTP client implementation components should use.

* [**Reference Browser**](https://github.com/mozilla-mobile/reference-browser)
  * Integrated crash reporting with [`lib-crash`](https://github.com/mozilla-mobile/android-components/tree/master/components/lib/crash).
  * Added awesome bar with [`browser-awesomebar`](https://github.com/mozilla-mobile/android-components/tree/master/components/browser/awesomebar).
  * Toolbar is hiding automatically now when scrolling web content.
  * Added "Sync Now" button to preferences (without functionality in this release)
  * Updated theme colors.

# 0.30.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.29.0...v0.30.0),
[Milestone](https://github.com/mozilla-mobile/android-components/milestone/32?closed=1),
[API reference](https://mozilla-mobile.github.io/android-components/api/0.30.0/index)

* Compiled against:
  * Android (SDK: 27, Support Libraries: 27.1.1)
  * Kotlin (Stdlib: 1.2.71, Coroutines: 0.30.2)
  * GeckoView (Nightly: 65.0.20181023100123, Beta: 64.0.20181022150107, Release: 63.0.20181018182531)
* **concept-storage**
  * ⚠️ **These are a breaking API changes**
  * Added a `getSuggestions` method to `HistoryStorage`, which is intended to power search, autocompletion, etc.
  * Added a `cleanup` method to `HistoryStorage`, which is intended to allow signaling to implementations to cleanup any allocated resources.
  * `HistoryStorage` methods `recordVisit` and `recordObservation` are now `suspend`.
  * `HistoryStorage` methods `getVisited()` and `getVisited(uris)` now return `Deferred`.
* 🆕 Added **browser-storage-memory** ✨
  * Added an in-memory implementation of `concept-storage`.
* 🆕 Added **browser-storage-sync** ✨
  * Added an implementation of `concept-storage` which is backed by the Rust Places library provided by [application-services](https://github.com/mozilla/application-services).
* **service-firefox-accounts**:
  * ⚠️ **This is a breaking API change**
  * The `FxaResult` type served as a custom promise-like type to support older versions of Java. We have now removed this type and switched to Kotlin's `Deferred` instead. We've also made sure all required types are `Closeable`:

  ```kotlin
  // Before
  Config.custom(CONFIG_URL).then { config: Config ->
    account = FirefoxAccount(config, CLIENT_ID, REDIRECT_URL)
  }

  // Now
  val account = async {
    Config.custom(CONFIG_URL).await().use { config ->
      FirefoxAccount(config, CLIENT_ID, REDIRECT_URL)
    }
  }
  ```
  In case error handling is needed, the new API will also become easier to work with:
  ```kotlin
  // Before
  account.beginOAuthFlow(scopes, wantsKeys).then({ url ->
    showLoginScreen(url)
  }, { exception ->
    handleException(exception)
  })

  // Now
  async {
      try {
        account.beginOAuthFlow(scopes, wantsKeys).await()
      } catch (e: FxaException) {
        handleException(e)
      }
  }

  ```
* **browser-engine-system, browser-engine-gecko, browser-engine-gecko-beta and browser-engine-gecko-nightly**:
  Adding support for using `SystemEngineView` and `GeckoEngineView` in a `CoordinatorLayout`.
  This allows to create nice transitions like hiding the toolbar when scrolls.
* **browser-session**
  * Fixed an issue where a custom tab `Session?` could get selected after removing the currently selected `Session`.
* **browser-toolbar**:
  * Added TwoStateButton that will change drawables based on the `isEnabled` listener. This is particularly useful for
  having a reload/cancel button.

  ```kotlin
  var isLoading: Boolean // updated by some state change.
  BrowserToolbar.TwoStateButton(
      reloadDrawable,
      "reload button",
      cancelDrawable,
      "cancel button",
      { isLoading }
  ) { /* On-click listener */ }
  ```
  * ⚠️ **These are a breaking API changes:** BrowserToolbar APIs for Button and ToggleButton have also been updated to accept `Drawable` instead of resource IDs.

  ```kotlin
  // Before
  BrowserToolbar.Button(R.drawable.image, "image description") {
    // perform an action on click.
  }

  // Now
  val imageDrawable: Drawable = Drawable()
  BrowserToolbar.Button(imageDrawable, "image description") {
    // perform an action on click.
  }

  // Before
  BrowserToolbar.ToggleButton(
    R.drawable.image,
    R.drawable.image_selected,
    "image description",
    "image selected description") {
    // perform an action on click.
  }

  // Now
  val imageDrawable: Drawable = Drawable()
  val imageSelectedDrawable: Drawable = Drawable()
  BrowserToolbar.ToggleButton(
    imageDrawable,
    imageSelectedDrawable,
    "image description",
    "image selected description") {
    // perform an action on click.
  }
  ```
* **concept-awesomebar**
  * 🆕 New component: An abstract definition of an awesome bar component.
* **browser-awesomebar**
  * 🆕 New component: A customizable [Awesome Bar](https://support.mozilla.org/en-US/kb/awesome-bar-search-firefox-bookmarks-history-tabs) implementation for browsers.A
* **feature-awesomebar**
  * 🆕 New component: A component that connects a [concept-awesomebar](https://github.com/mozilla-mobile/android-components/components/concept/awesomebar/README.md) implementation to a [concept-toolbar](https://github.com/mozilla-mobile/android-components/components/concept/toolbar/README.md) implementation and provides implementations of various suggestion providers.

# 0.29.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.28.0...v0.29.0),
[Milestone](https://github.com/mozilla-mobile/android-components/milestone/31?closed=1),
[API reference](https://mozilla-mobile.github.io/android-components/api/0.29.0/index)

* Compiled against:
  * Android (SDK: 27, Support Libraries: 27.1.1)
  * Kotlin (Stdlib: **1.2.71** 🔺, Coroutines: **0.30.2** 🔺)
  * GeckoView (Nightly: **65.0.20181023100123** 🔺, Beta: **64.0.20181022150107** 🔺, Release: **63.0.20181018182531** 🔺)
* **browser-toolbar**:
  * Added new listener to get notified when the user is editing the URL:
  ```kotlin
  toolbar.setOnEditListener(object : Toolbar.OnEditListener {
      override fun onTextChanged(text: String) {
          // Fired whenever the user changes the text in the address bar.
      }

      override fun onStartEditing() {
          // Fired when the toolbar switches to edit mode.
      }

      override fun onStopEditing() {
          // Fired when the toolbar switches back to display mode.
      }
  })
  ```
  * Added new toolbar APIs:
  ```kotlin
  val toolbar = BrowserToolbar(context)
  toolbar.textColor: Int = getColor(R.color.photonRed50)
  toolbar.hintColor: Int = getColor(R.color.photonGreen50)
  toolbar.textSize: Float = 12f
  toolbar.typeface: Typeface = Typeface.createFromFile("fonts/foo.tff")
  ```
  These attributes are also available in XML (except for typeface):

    ```xml
    <mozilla.components.browser.toolbar.BrowserToolbar
      android:id="@+id/toolbar"
      app:browserToolbarTextColor="#ff0000"
      app:browserToolbarHintColor="#00ff00"
      app:browserToolbarTextSize="12sp"
      android:layout_width="match_parent"
      android:layout_height="wrap_content"/>
    ```

  * [API improvement](https://github.com/mozilla-mobile/android-components/issues/772) for more flexibility to create a `BrowserToolbar.Button`,
  and `BrowserToolbar.ToggleButton`, now you can provide a custom padding:
  ```kotlin
  val padding = Padding(start = 16, top = 16, end = 16, bottom = 16)
  val button = BrowserToolbar.Button(mozac_ic_back, "Forward", padding = padding) {}
  var toggle = BrowserToolbar.ToggleButton(mozac_ic_pin, mozac_ic_pin_filled, "Pin", "Unpin", padding = padding) {}
  ```
* **concept-toolbar**:
  * [API improvement](https://github.com/mozilla-mobile/android-components/issues/772) for more flexibility to create a `Toolbar.ActionToggleButton`,
  `Toolbar.ActionButton`, `Toolbar.ActionSpace` and `Toolbar.ActionImage`, now you can provide a custom padding:
  ```kotlin
  val padding = Padding(start = 16, top = 16, end = 16, bottom = 16)
  var toggle = Toolbar.ActionToggleButton(0, mozac_ic_pin_filled, "Pin", "Unpin", padding = padding) {}
  val button = Toolbar.ActionButton(mozac_ic_back, "Forward", padding = padding) {}
  val space = Toolbar.ActionSpace(pxToDp(128), padding = padding)
  val image = Toolbar.ActionImage(brand, padding = padding)
  ```
* **support-base**:
  * A new class add for representing an Android Padding.
    ```kotlin
    val padding = Padding(16, 24, 32, 40)
    val (start, top, end, bottom) = padding
    ```
* **support-ktx**:
  * A new extention function that allows you to set `Padding` object to a `View`.
    ```kotlin
    val padding = Padding(16, 24, 32, 40)
    val view = View(context)
    view.setPadding(padding)
    ```
* **concept-engine**, **browser-engine-system**, **browser-engine-gecko(-beta/nightly)**
  * `RequestInterceptor` was enhanced to support loading an alternative URL.
  :warning: **This is a breaking change for the `RequestInterceptor` method signature!**
  ```kotlin
  // To provide alternative content the new InterceptionResponse.Content type needs to be used
  requestInterceptor = object : RequestInterceptor {
      override fun onLoadRequest(session: EngineSession, uri: String): InterceptionResponse? {
          return when (uri) {
              "sample:about" -> InterceptionResponse.Content("<h1>I am the sample browser</h1>")
              else -> null
          }
      }
  }
  // To provide an alternative URL the new InterceptionResponse.Url type needs to be used
  requestInterceptor = object : RequestInterceptor {
      override fun onLoadRequest(session: EngineSession, uri: String): InterceptionResponse? {
          return when (uri) {
              "sample:about" -> InterceptionResponse.Url("sample:aboutNew")
              else -> null
          }
      }
  }
  ```
* **concept-storage**:
  * Added a new concept for describing an interface for storing browser data. First iteration includes a description of `HistoryStorage`.
* **feature-storage**:
  * Added a first iteration of `feature-storage`, which includes `HistoryTrackingFeature` that ties together `concept-storage` and `concept-engine` and allows engines to track history visits and page meta information. It does so by implementing `HistoryTrackingDelegate` defined by `concept-engine`.
  Before adding a first session to the engine, initialize the history tracking feature:
  ```kotlin
  val historyTrackingFeature = HistoryTrackingFeature(
      components.engine,
      components.historyStorage
  )
  ```
  Once the feature has been initialized, history will be tracked for all subsequently added sessions.
* **sample-browser**:
  * Updated the sample browser to track browsing history using an in-memory history storage implementation (how much is actually tracked in practice depends on which engine is being used. As of this release, only `SystemEngine` provides a full set of necessary APIs).
* **lib-crash**
  * Added option to display additional message in prompt and define the theme to be used:
  ```kotlin
  CrashReporter(
      promptConfiguration = CrashReporter.PromptConfiguration(
        // ..

        // An additional message that will be shown in the prompt
        message = "We are very sorry!"

        // Use a custom theme for the prompt (Extend Theme.Mozac.CrashReporter)
        theme = android.R.style.Theme_Holo_Dialog
      )
      // ..
  ).install(applicationContext)
  ```
  * Showing the crash prompt won't play the default activity animation anymore.
  * Added a new sample app `samples-crash` to show and test crash reporter integration.
* **feature-tabs**:
  * `TabsToolbarFeature` is now adding a `TabCounter` from the `ui-tabcounter` component to the toolbar.
* **lib-jexl**
  * New component for evaluating Javascript Expression Language (JEXL) expressions. This implementation is based on [Mozjexl](https://github.com/mozilla/mozjexl) used at Mozilla, specifically as a part of SHIELD and Normandy. In a future version of Fretboard JEXL will allow more complex rules for experiments. For more see [documentation](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/jexl/README.md).
* **service-telemetry**
  * Added option to send list of experiments in event pings: `Telemetry.recordExperiments(Map<String, Boolean> experiments)`
  * Fixed an issue where `DebugLogClient` didn't use the provided log tag.
* **service-fretboard**
  * Fixed an issue where for some locales a `MissingResourceException` would occur.
* **browser-engine-system**
  * Playback of protected media (DRM) is now granted automatically.
* **browser-engine-gecko**
  * Updated components to follow merge day: (Nightly: 65.0, Beta: 64.0, Release: 63.0)

# 0.28.0

Release date: 2018-10-23

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.27.0...v0.28.0),
[Milestone](https://github.com/mozilla-mobile/android-components/milestone/30?closed=1),
[API reference](https://mozilla-mobile.github.io/android-components/api/0.28.0/index)

⚠️ **Note**: This and upcoming releases are **only** available from *maven.mozilla.org*.

* Compiled against:
  * Android (SDK: 27, Support Libraries: 27.1.1)
  * Kotlin (Stdlib: 1.2.61, Coroutines: 0.23.4)
  * GeckoView
    * Nightly: 64.0.20181004100221
    * Beta: 63.0b3 (0269319281578bff4e01d77a21350bf91ba08620)
    * Release: 62.0 (9cbae12a3fff404ed2c12070ad475424d0ae869f)
* **concept-engine**
  * Added `HistoryTrackingDelegate` interface for integrating engine implementations with history storage backends. Intended to be used via engine settings.
* **browser-engine**
  * `Download.fileName` cannot be `null` anymore. All engine implementations are guaranteed to return a proposed file name for Downloads now.
* **browser-engine-gecko-***, **browser-engine-system**
  * Added support for `HistoryTrackingDelegate`, if it's specified in engine settings.
* **browser-engine-servo**
  * Added a new experimental *Engine* implementation based on the [Servo Browser Engine](https://servo.org/).
* **browser-session** - basic session hierarchy:
  * Sessions can have a parent `Session` now. A `Session` with a parent will be added *after* the parent `Session`. On removal of a selected `Session` the parent `Session` can be selected automatically if desired:
  ```kotlin
  val parent = Session("https://www.mozilla.org")
  val session = Session("https://www.mozilla.org/en-US/firefox/")

  sessionManager.add(parent)
  sessionManager.add(session, parent = parent)

  sessionManager.remove(session, selectParentIfExists = true)
  ```
* **browser-session** - obtaining an restoring a SessionsSnapshot:
  * It's now possible to request a SessionsSnapshot from the `SessionManager`, which encapsulates currently active sessions, their order and state, and which session is the selected one. Private and Custom Tab sessions are omitted from the snapshot. A new public `restore` method allows restoring a `SessionsSnapshot`.
  ```kotlin
  val snapshot = sessionManager.createSnapshot()
  // ... persist snapshot somewhere, perhaps using the DefaultSessionStorage
  sessionManager.restore(snapshot)
  ```
  * `restore` follows a different observer notification pattern from regular `add` flow. See method documentation for details. A new `onSessionsRestored` notification is now available.
* **browser-session** - new SessionStorage API, new DefaultSessionStorage data format:
  * Coupled with the `SessionManager` changes, the SessionStorage API has been changed to operate over `SessionsSnapshot`. New API no longer operates over a SessionManager, and instead reads/writes snapshots which may used together with the SessionManager (see above). An explicit `clear` method is provided for wiping SessionStorage.
  * `DefaultSessionStorage` now uses a new storage format internally, which allows maintaining session ordering and preserves session parent information.
* **browser-errorpages**
  * Added translation annotations to our error page strings. Translated strings will follow in a future release.
* **service-glean**
  * A new client-side telemetry SDK for collecting metrics and sending them to Mozilla's telemetry service. This component is going to eventually replace `service-telemetry`. The SDK is currently in development and the component is not ready to be used yet.
* **lib-dataprotect**
  * The `Keystore` class and its `encryptBytes()` and `decryptBytes()` methods are now open to simplify mocking in unit tests.
* **ui-tabcounter**
  * The `TabCounter` class is now open and can get extended.
* **feature-downloads**
  * Now you're able to provide a dialog before a download starts and customize it to your wish. Take a look at the [updated docs](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/downloads/README.md).

# 0.27.0

Release date: 2018-10-16

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.26.0...v0.27.0),
[Milestone](https://github.com/mozilla-mobile/android-components/milestone/27?closed=1),
[API reference](https://mozilla-mobile.github.io/android-components/api/0.27.0/index)

* Compiled against:
  * Android (SDK: 27, Support Libraries: 27.1.1)
  * Kotlin (Stdlib: 1.2.61, Coroutines: 0.23.4)
  * GeckoView
    * Nightly: **64.0.20181004100221** 🔺
    * Beta: 63.0b3 (0269319281578bff4e01d77a21350bf91ba08620)
    * Release: 62.0 (9cbae12a3fff404ed2c12070ad475424d0ae869f)
* **browser-engine-system**
  * Fixed a bug where `SystemEngineSession#exitFullScreenMode` didn't invoke the internal callback to exit the fullscreen mode.
  * A new field `defaultUserAgent` was added to `SystemEngine` for testing purposes. This is to circumvent calls to `WebSettings.getDefaultUserAgent` which fails with a `NullPointerException` in Robolectric. If the `SystemEngine` is used in Robolectric tests the following code will be needed:
    ```kotlin
    @Before
    fun setup() {
        SystemEngine.defaultUserAgent = "test-ua-string"
    }
    ```
* **browser-engine-gecko-nightly**:
  * Enabled [Java 8 support](https://developer.android.com/studio/write/java8-support) to meet upstream [GeckoView requirements](https://mail.mozilla.org/pipermail/mobile-firefox-dev/2018-September/002411.html). Apps using this component need to enable Java 8 support as well:
  ```Groovy
  android {
    ...
    compileOptions {
      sourceCompatibility JavaVersion.VERSION_1_8
      targetCompatibility JavaVersion.VERSION_1_8
    }
  }
  ```
* **browser-search**
  * Fixed an issue where a locale change at runtime would not update the search engines.
* **browser-session**:
  * Added reusable functionality for observing sessions, which also support observering the currently selected session, even if it changes.
  ```kotlin
  class MyFeaturePresenter(
      private val sessionManager: SessionManager
  ) : SelectionAwareSessionObserver(sessionManager) {

      fun start() {
          // Always observe changes to the selected session even if the selection changes
          super.observeSelected()

          // To observe changes to a specific session the following method can be used:
          // super.observeFixed(session)
      }

      override fun onUrlChanged(session: Session, url: String) {
          // URL of selected session changed
      }

      override fun onProgress(session: Session, progress: Int) {
         // Progress of selected session changed
      }

      // More observer functions...
  }
  ```
* **browser-errorpages**
  * Added more detailed documentation in the README.
* **feature-downloads**
  * A new components for apps that want to process downloads, for more examples take a look at [here](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/downloads/README.md).
* **lib-crash**
  * A new generic crash reporter component that can report crashes to multiple services ([documentation](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/crash/README.md)).
* **support-ktx**
  * Added new helper method to run a block of code with a different StrictMode policy:
  ```kotlin
  StrictMode.allowThreadDiskReads().resetAfter {
    // In this block disk reads are not triggering a strict mode violation
  }
  ```
  * Added a new helper for checking if you have permission to do something or not:
  ```kotlin
    var isGranted = context.isPermissionGranted(INTERNET)
    if (isGranted) {
        //You can proceed
    } else {
        //Request permission
    }
  ```
* **support-test**
  * Added a new helper for granting permissions in  Robolectric tests:
  ```kotlin
     val context = RuntimeEnvironment.application
     var isGranted = context.isPermissionGranted(INTERNET)

     assertFalse(isGranted) //False permission is not granted yet.

     grantPermission(INTERNET) // Now you have permission.

     isGranted = context.isPermissionGranted(INTERNET)

     assertTrue(isGranted) // True :D
  ```

# 0.26.0

Release date: 2018-10-05

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.25.1...v0.26.0),
[Milestone](https://github.com/mozilla-mobile/android-components/milestone/26?closed=1),
[API reference](https://mozilla-mobile.github.io/android-components/api/0.26.0/index)

* Compiled against:
  * Android (SDK: 27, Support Libraries: 27.1.1)
  * Kotlin (Stdlib: 1.2.61, Coroutines: 0.23.4)
  * GeckoView
    * Nightly: 64.0.20180905100117
    * Beta: 63.0b3 (0269319281578bff4e01d77a21350bf91ba08620)
    * Release: 62.0 (9cbae12a3fff404ed2c12070ad475424d0ae869f)

* ⚠️ **Releases are now getting published on [maven.mozilla.org](http://maven.mozilla.org/?prefix=maven2/org/mozilla/components/)**.
  * Additionally all artifacts published now use an artifact name that matches the gradle module name (e.g. `browser-toolbar` instead of just `toolbar`).
  * All artifcats are published with the group id `org.mozilla.components` (`org.mozilla.photon` is not being used anymore).
  * For a smooth transition all artifacts still get published on JCenter with the old group ids and artifact ids. In the near future releases will only be published on maven.mozilla.org. Old releases will remain on JCenter and not get removed.
* **browser-domains**
  * Removed `microsoftonline.com` from the global and localized domain lists. No content is being served from that domain. Only subdomains like `login.microsoftonline.com` are used.
* **browser-errorpages**
  * Added error page support for multiple error types.
    ```kotlin
    override fun onErrorRequest(
        session: EngineSession,
        errorType: ErrorType, // This used to be an Int
        uri: String?
    ): RequestInterceptor.ErrorResponse? {
        // Create an error page.
        val errorPage = ErrorPages.createErrorPage(context, errorType)
        // Return it to the request interceptor to take care of default error cases.
        return RequestInterceptor.ErrorResponse(errorPage)
    }
    ```
  * :warning: **This is a breaking change for the `RequestInterceptor#onErrorRequest` method signature!**
* **browser-engine-**
  * Added a setting for enabling remote debugging.
  * Creating an `Engine` requires a `Context` now.
      ```kotlin
       val geckoEngine = GeckoEngine(context)
       val systemEngine = SystemEngine(context)
      ```
* **browser-engine-system**
  * The user agent string now defaults to WebView's default, if not provided, and to the user's default, if provided. It can also be read and changed:
    ```kotlin
    // Using WebView's default
    val engine = SystemEngine(context)

    // Using customized WebView default
    val engine = SystemEngine(context)
    engine.settings.userAgentString = buildUserAgentString(engine.settings.userAgentString)

    // Using custom default
    val engine = SystemEngine(context, DefaultSettings(userAgentString = "foo"))
    ```
  * The tracking protection policy can now be set, both as a default and at any time later.
    ```kotlin
    // Set the default tracking protection policy
    val engine = SystemEngine(context, DefaultSettings(
      trackingProtectionPolicy = TrackingProtectionPolicy.all())
    )

    // Change the tracking protection policy
    engine.settings.trackingProtectionPolicy = TrackingProtectionPolicy.select(
      TrackingProtectionPolicy.AD,
      TrackingProtectionPolicy.SOCIAL
    )
    ```
* **browser-engine-gecko(-*)**
  * Creating a `GeckoEngine` requires a `Context` now. Providing a `GeckoRuntime` is now optional.
* **browser-session**
  * Fixed an issue that caused a Custom Tab `Session` to get selected if it is the first session getting added.
  * `Observer` instances that get attached to a `LifecycleOwner` can now automatically pause and resume observing whenever the lifecycle pauses and resumes. This behavior is off by default and can be enabled by using the `autoPause` parameter when registering the `Observer`.
    ```kotlin
    sessionManager.register(
        observer = object : SessionManager.Observer {
            // ...
        },
        owner = lifecycleOwner,
        autoPause = true
    )
    ```
  * Added an optional callback to provide a default `Session` whenever  `SessionManager` is empty:
    ```kotlin
    val sessionManager = SessionManager(
        engine,
        defaultSession = { Session("https://www.mozilla.org") }
    )
    ```
* **service-telemetry**
  * Added `Telemetry.getClientId()` to let consumers read the client ID.
  * `Telemetry.recordSessionEnd()` now takes an optional callback to be executed upon failure - instead of throwing `IllegalStateException`.
* **service-fretboard**
  * Added `ValuesProvider.getClientId()` to let consumers specify the client ID to be used for bucketing the client. By default fretboard will generate and save an internal UUID used for bucketing. By specifying the client ID consumers can use the same ID for telemetry and bucketing.
  * Update jobs scheduled with `WorkManagerSyncScheduler` will now automatically retry if the configuration couldn't get updated.
  * The update interval of `WorkManagerSyncScheduler` can now be configured.
  * Fixed an issue when reading a corrupt experiments file from disk.
  * Added a workaround for HttpURLConnection throwing ArrayIndexOutOfBoundsException.
* **ui-autocomplete**
  * Fixed an issue causing desyncs between the soft keyboard and `InlineAutocompleteEditText`.
* **samples-firefox-accounts**
  * Showcasing new pairing flow which allows connecting new devices to existing accounts using a QR code.

# 0.25.1 (2018-09-27)

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.25...v0.25.1),
[Milestone](https://github.com/mozilla-mobile/android-components/milestone/28?closed=1),
[API reference](https://mozilla-mobile.github.io/android-components/api/0.25.1/index)

* Compiled against:
  * Android
    * SDK: 27
    * Support Libraries: 27.1.1
  * Kotlin
    * Standard library: 1.2.61
    * Coroutines: 0.23.4
  * GeckoView
    * Nightly: 64.0.20180905100117
    * Beta: 63.0b3 (0269319281578bff4e01d77a21350bf91ba08620)
    * Release: 62.0 (9cbae12a3fff404ed2c12070ad475424d0ae869f)

* **browser-engine-system**: Fixed a `NullPointerException` in `SystemEngineSession.captureThumbnail()`.

# 0.25 (2018-09-26)

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.24...v0.25),
[Milestone](https://github.com/mozilla-mobile/android-components/milestone/25?closed=1),
[API reference](https://mozilla-mobile.github.io/android-components/api/0.25/index)

* Compiled against:
  * Android
    * SDK: 27
    * Support Libraries: 27.1.1
  * Kotlin
    * Standard library: 1.2.61
    * Coroutines: 0.23.4
  * GeckoView
    * Nightly: 64.0.20180905100117
    * Beta: 63.0b3 (0269319281578bff4e01d77a21350bf91ba08620)
    * Release: 62.0 (9cbae12a3fff404ed2c12070ad475424d0ae869f)

* ⚠️ **This is the last release compiled against Android SDK 27. Upcoming releases of the components will require Android SDK 28**.
* **service-fretboard**:
  * Fixed a bug in `FlatFileExperimentStorage` that caused updated experiment configurations not being saved to disk.
  * Added [WorkManager](https://developer.android.com/reference/kotlin/androidx/work/WorkManager) implementation for updating experiment configurations in the background (See ``WorkManagerSyncScheduler``).
  * `Experiment.id` is not accessible by component consumers anymore.
* **browser-engine-system**:
  * URL changes are now reported earlier; when the URL of the main frame changes.
  * Fixed an issue where fullscreen mode would only take up part of the screen.
  * Fixed a crash that could happen when loading invalid URLs.
  * `RequestInterceptor.onErrorRequest()` can return custom error page content to be displayed now (the original URL that caused the error will be preserved).
* **feature-intent**: New component providing intent processing functionality (Code moved from *feature-session*).
* **support-utils**: `DownloadUtils.guessFileName()` will replace extension in the URL with the MIME type file extension if needed (`http://example.com/file.aspx` + `image/jpeg` -> `file.jpg`).

# 0.24 (2018-09-21)

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.23...v0.24),
[Milestone](https://github.com/mozilla-mobile/android-components/milestone/24?closed=1),
[API reference](https://mozilla-mobile.github.io/android-components/api/0.24/index)

* Compiled against:
  * Android
    * SDK: 27
    * Support Libraries: 27.1.1
  * Kotlin
    * Standard library: 1.2.61
    * Coroutines: 0.23.4
  * GeckoView
    * Nightly: 64.0.20180905100117
    * Beta: 63.0b3 (0269319281578bff4e01d77a21350bf91ba08620)
    * Release: 62.0 (9cbae12a3fff404ed2c12070ad475424d0ae869f)

* **dataprotect**:
  * Added a component using AndroidKeyStore to protect user data.
  ```kotlin
  // Create a Keystore and generate a key
  val keystore: Keystore = Keystore("samples-dataprotect")
  keystore.generateKey()

  // Encrypt data
  val plainText = "plain text data".toByteArray(StandardCharsets.UTF_8)
  val encrypted = keystore.encryptBytes(plain)

  // Decrypt data
  val samePlainText = keystore.decryptBytes(encrypted)
  ```
* **concept-engine**: Enhanced settings to cover most common WebView settings.
* **browser-engine-system**:
  * `SystemEngineSession` now provides a way to capture a screenshot of the actual content of the web page just by calling `captureThumbnail`
* **browser-session**:
  * `Session` exposes a new property called `thumbnail` and its internal observer also exposes a new listener `onThumbnailChanged`.

  ```Kotlin
  session.register(object : Session.Observer {
      fun onThumbnailChanged(session: Session, bitmap: Bitmap?) {
              // Do Something
      }
  })
  ```

  * `SessionManager` lets you notify it when the OS is under low memory condition by calling to its new function `onLowMemory`.

* **browser-tabstray**:

   * Now on `BrowserTabsTray` every tab gets is own thumbnail :)

* **support-ktx**:

   * Now you can easily query if the OS is under low memory conditions, just by using `isOSOnLowMemory()` extention function on `Context`.

  ```Kotlin
  val shouldReduceMemoryUsage = context.isOSOnLowMemory()

  if (shouldReduceMemoryUsage) {
      //Deallocate some heavy objects
  }
  ```

  * `View.dp` is now`Resource.pxtoDp`.

  ```Kotlin
  // Before
  toolbar.dp(104)

  // Now
  toolbar.resources.pxToDp(104)
  ```
* **samples-browser**:
   * Updated to show the new features related to tab thumbnails. Be aware that this feature is only available for `systemEngine` and you have to switch to the build variant `systemEngine*`.

# 0.23 (2018-09-13)

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.22...v0.23),
[Milestone](https://github.com/mozilla-mobile/android-components/milestone/23?closed=1),
[API reference](https://mozilla-mobile.github.io/android-components/api/0.23/index)

* Compiled against:
  * Android
    * SDK: 27
    * Support Libraries: 27.1.1
  * Kotlin
    * Standard library: 1.2.61
    * Coroutines: 0.23.4
  * GeckoView
    * Nightly: 64.0.20180905100117
    * Beta: 63.0b3 (0269319281578bff4e01d77a21350bf91ba08620)
    * Release: 62.0 (9cbae12a3fff404ed2c12070ad475424d0ae869f)

* Added initial documentation for the browser-session component: https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/README.md
* **sync-logins**: New component for integrating with Firefox Sync (for Logins). A sample app showcasing this new functionality can be found at: https://github.com/mozilla-mobile/android-components/tree/master/samples/sync-logins
* **browser-engine-**:
  * Added support for fullscreen mode and the ability to exit it programmatically if needed.
  ```Kotlin
  session.register(object : Session.Observer {
      fun onFullScreenChange(enabled: Boolean) {
          if (enabled) {
              // ..
              sessionManager.getEngineSession().exitFullScreenMode()
          }
      }
  })
  ```
* **concept-engine**, **browser-engine-system**, **browser-engine-gecko(-beta/nightly)**:
  * We've extended support for intercepting requests to also include intercepting of errors
  ```Kotlin
  val interceptor = object : RequestInterceptor {
    override fun onErrorRequest(
      session: EngineSession,
      errorCode: Int,
      uri: String?
    ) {
      engineSession.loadData("<html><body>Couldn't load $uri!</body></html>")
    }
  }
  // GeckoEngine (beta/nightly) and SystemEngine support request interceptors.
  GeckoEngine(runtime, DefaultSettings(requestInterceptor = interceptor))
  ```
* **browser-engine-system**:
    * Added functionality to clear all browsing data
    ```Kotlin
    sessionManager.getEngineSession().clearData()
    ```
    * `onNavigationStateChange` is now called earlier (when the title of a web page is available) to allow for faster toolbar updates.
* **feature-session**: Added support for processing `ACTION_SEND` intents (`ACTION_VIEW` was already supported)

  ```Kotlin
  // Triggering a search if the provided EXTRA_TEXT is not a URL
  val searchHandler: TextSearchHandler = { searchTerm, session ->
       searchUseCases.defaultSearch.invoke(searchTerm, session)
  }

  // Handles both ACTION_VIEW and ACTION_SEND intents
  val intentProcessor = SessionIntentProcessor(
      sessionUseCases, sessionManager, textSearchHandler = searchHandler
  )
  intentProcessor.process(intent)
  ```
* Replaced some miscellaneous uses of Java 8 `forEach` with Kotlin's for consistency and backward-compatibility.
* Various bug fixes (see [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.22...v0.23) for details).

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
* **browser-engine-**:
  * EngineView now exposes lifecycle methods with default implementations. A `LifecycleObserver` implementation is provided which forwards events to EngineView instances.
  ```kotlin
  lifecycle.addObserver(EngineView.LifecycleObserver(view))
  ```
  * Added engine setting for blocking web fonts:
  ```kotlin
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

